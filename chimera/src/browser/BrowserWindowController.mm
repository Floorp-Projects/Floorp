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

#import "BrowserWrapper.h"
#import "BrowserContentViews.h"
#import "PreferenceManager.h"
#import "BookmarksDataSource.h"
#import "HistoryDataSource.h"
#import "BrowserTabView.h"
#import "UserDefaults.h"
#import "PageProxyIcon.h"

#include "nsIWebNavigation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIPrefBranch.h"
#include "nsIContextMenuListener.h"
#include "nsIDOMWindow.h"
#include "CHBrowserService.h"
#include "nsString.h"
#include "nsCRT.h"
#include "GeckoUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChrome.h"

#include "nsIClipboardCommands.h"
#include "nsICommandManager.h"
#include "nsIWebBrowser.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManagerUtils.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIBrowserHistory.h"

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
static NSString *ViewSourceToolbarItemIdentifier = @"View Source Toolbar Item";
static NSString *BookmarkToolbarItemIdentifier = @"Bookmark Toolbar Item";
static NSString *TextBiggerToolbarItemIdentifier = @"Text Bigger Toolbar Item";
static NSString *TextSmallerToolbarItemIdentifier = @"Text Smaller Toolbar Item";
static NSString *NewTabToolbarItemIdentifier = @"New Tab Toolbar Item";
static NSString *CloseTabToolbarItemIdentifier = @"Close Tab Toolbar Item";
static NSString *SendURLToolbarItemIdentifier = @"Send URL Toolbar Item";


static NSString *NavigatorWindowFrameSaveName = @"NavigatorWindow";

// Cached toolbar defaults read in from a plist. If null, we'll use
// hardcoded defaults.
static NSArray* sToolbarDefaults = nil;

#define kMaxBrowserWindowTabs 16

//////////////////////////////////////
@interface AutoCompleteTextFieldEditor : NSTextView
{
}
@end

@implementation AutoCompleteTextFieldEditor

-(void)paste:(id)sender
{
  NSPasteboard *pboard = [NSPasteboard generalPasteboard];
  NSEnumerator *dataTypes = [[pboard types] objectEnumerator];
  NSString *aType;
  while ((aType = [dataTypes nextObject])) {
    if ([aType isEqualToString:NSStringPboardType]) {
      NSString *oldText = [pboard stringForType:NSStringPboardType];
      NSString *newText = [oldText stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
      NSRange aRange = [self selectedRange];
      if ([self shouldChangeTextInRange:aRange replacementString:newText]) {
        [[self textStorage] replaceCharactersInRange:aRange withString:newText];
        [self didChangeText];
      }
      // after a paste, the insertion point should be after the pasted text
      unsigned int newInsertionPoint = aRange.location + [newText length];
      [self setSelectedRange:NSMakeRange(newInsertionPoint,0)];
      return;
    }
  }
}

@end
//////////////////////////////////////


@interface BrowserWindowController(Private)
- (void)setupToolbar;
- (void)setupSidebarTabs;
- (NSString*)getContextMenuNodeDocumentURL;
- (void)loadSourceOfURL:(NSString*)urlStr;
@end

@implementation BrowserWindowController

- (id)initWithWindowNibName:(NSString *)windowNibName
{
  if ( (self = [super initWithWindowNibName:(NSString *)windowNibName]) )
  {
    // we cannot rely on the OS to correctly cascade new windows (RADAR bug 2972893)
    // so we turn off the cascading. We do it at the end of |windowDidLoad|
    [self setShouldCascadeWindows:NO];
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
    mURLFieldEditor = nil;
    mProgressSuperview = nil;
    mBookmarkToolbarItem = nil;
    mSidebarToolbarItem = nil;
  
    // register for services
    NSArray* sendTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    NSArray* returnTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    [NSApp registerServicesMenuSendTypes:sendTypes returnTypes:returnTypes];
    
    nsCOMPtr<nsIBrowserHistory> globalHist = do_GetService("@mozilla.org/browser/global-history;1");
    mGlobalHistory = globalHist;
    if ( mGlobalHistory )
      NS_ADDREF(mGlobalHistory);
    nsCOMPtr<nsIURIFixup> fixer ( do_GetService("@mozilla.org/docshell/urifixup;1") );
    mURIFixer = fixer;
    if ( fixer )
      NS_ADDREF(mURIFixer);
  }
  return self;
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
  mClosingWindow = YES;
    
#if DEBUG
  NSLog(@"Window will close notification.");
#endif
  [mSidebarBookmarksDataSource windowClosing];

  [self autosaveWindowFrame];
  
  { // scope...
    nsCOMPtr<nsIRDFRemoteDataSource> dataSource ( do_QueryInterface(mGlobalHistory) );
    if (dataSource)
      dataSource->Flush();
    NS_IF_RELEASE(mGlobalHistory);
    NS_IF_RELEASE(mURIFixer);
  } // matters
  
  // Loop over all tabs, and tell them that the window is closed. This
  // stops gecko from going any further on any of its open connections
  // and breaks all the necessary cycles between Gecko and the BrowserWrapper.
  int numTabs = [mTabBrowser numberOfTabViewItems];
  for (int i = 0; i < numTabs; i++) {
    NSTabViewItem* item = [mTabBrowser tabViewItemAtIndex: i];
    [[item view] windowClosed];
  }

  // autorelease just in case we're here because of a window closing
  // initiated from gecko, in which case this BWC would still be on the 
  // stack and may need to stay alive until it unwinds. We've already
  // shut down gecko above, so we can safely go away at a later time.
  [self autorelease];
}


- (void)dealloc
{
#if DEBUG
  NSLog(@"Browser controller died.");
#endif

  // active Gecko connections have already been shut down in |windowWillClose|
  // so we don't need to worry about that here. We only have to be careful
  // not to access anything related to the document, as it's been destroyed. The
  // superclass dealloc takes care of our child NSView's, which include the 
  // BrowserWrappers and their child CHBrowserViews.
  
  //if (mSidebarBrowserView)
  //  [mSidebarBrowserView windowClosed];

  [mProgress release];

  [self stopThrobber];
  [mThrobberImages release];
  [mURLFieldEditor release];
  
  [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    BOOL mustResizeChrome = NO;
    
    // hide the resize control if specified by the chrome mask
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
      [[self window] setShowsResizeIndicator:NO];
    
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) ) {
      // remove the status bar at the bottom
      [mStatusBar removeFromSuperview];
      mustResizeChrome = YES;
      
      // clear out everything in the status bar we were holding on to. This will cause us to
      // pass nil for these status items into the CHBrowserwWrapper which is what we want. We'll
      // crash if we give them things that have gone away.
      mProgress = nil;
      mStatus = nil;
      mLock = nil;
    }
    else {
      // Retain with a single extra refcount. This allows us to remove
      // the progress meter from its superview without having to worry
      // about retaining and releasing it. Cache the superview of the
      // progress. Dynamically fetch the superview so as not to burden
      // someone rearranging the nib with this detail.
      [mProgress retain];
      mProgressSuperview = [mProgress superview];
      
      // due to a cocoa issue with it updating the bounding box of two rects
      // that both needing updating instead of just the two individual rects
      // (radar 2194819), we need to make the text area opaque.
      [mStatus setBackgroundColor:[NSColor windowBackgroundColor]];
      [mStatus setDrawsBackground:YES];
    }

    // Get our saved dimensions.
    NSRect oldFrame = [[self window] frame];
    [[self window] setFrameUsingName: NavigatorWindowFrameSaveName];
    
    if (NSEqualSizes(oldFrame.size, [[self window] frame].size))
      mustResizeChrome = YES;
    
    mInitialized = YES;

    mDrawerCachedFrame = NO;
        
    [[self window] setAcceptsMouseMovedEvents: YES];
    
    [self setupToolbar];
    
    // set an upper limit on the number of tabs per window
    [mTabBrowser setMaxNumberOfTabs: kMaxBrowserWindowTabs];

//  03/03/2002 mlj Changing strategy a bit here.  The addTab: method was
//  duplicating a lot of the code found here.  I have moved it to that method.
//  We now remove the IB tab, then add one of our own.

    [mTabBrowser removeTabViewItem:[mTabBrowser tabViewItemAtIndex:0]];
    
    // create ourselves a new tab and fill it with the appropriate content. If we
    // have a URL pending to be opened here, don't load anything in it, otherwise,
    // load the homepage if that's what the user wants (or about:blank).
    [self createNewTab:(mPendingURL ? eNewTabEmpty : (mShouldLoadHomePage ? eNewTabHomepage : eNewTabAboutBlank))];
    
    // we have a url "pending" from the "open new window with link" command. Deal
    // with it now that everything is loaded.
    if (mPendingURL) {
      if (mShouldLoadHomePage)
        [self loadURL: mPendingURL referrer:mPendingReferrer activate:mPendingActivate];
      [mPendingURL release];
      [mPendingReferrer release];
      mPendingURL = mPendingReferrer = nil;
    }
    
    [mSidebarDrawer setDelegate: self];

    [self setupSidebarTabs];

    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR) ) {
      // remove the personal toolbar and adjust the content area upwards. Removing it
      // from the parent view releases it, so we have to clear out the member var.
      //float height = [mPersonalToolbar frame].size.height;
      [mPersonalToolbar removeFromSuperview];
      mPersonalToolbar = nil;
      mustResizeChrome = YES;
    }
    else
    {
      [mPersonalToolbar initializeToolbar];
    
      if (![self shouldShowBookmarkToolbar])
        [mPersonalToolbar showBookmarksToolbar:NO];
    }
    
    if (mustResizeChrome)
      [mContentView resizeSubviewsWithOldSize:[mContentView frame].size];
      
    // stagger window from last browser, if there is one. we can't just use autoposition
    // because it doesn't work on multiple monitors (radar bug 2972893). |getFrontmostBrowserWindow|
    // only gets fully chromed windows, so this will do the right thing for popups (yay!).
    NSWindow* lastBrowser = [[NSApp delegate] getFrontmostBrowserWindow];
    if ( lastBrowser != [self window] ) {
      NSRect lastBrowserFrame = [lastBrowser frame];
      NSPoint topLeft = NSMakePoint(NSMinX(lastBrowserFrame), NSMaxY(lastBrowserFrame));
      topLeft.x += 15; topLeft.y -= 15;
      [[self window] setFrameTopLeftPoint:topLeft];
      
      // check if this new topLeft will overlap the dock or go off the screen, if so,
      // force to 0,0 of the current monitor. We test this by unioning the window rect
      // with the visible screen rect (excluding dock). If the result isn't the same
      // as the screen rect, the window juts out somewhere and needs to be repositioned.
      NSRect newBrowserFrame = [[self window] frame];
      NSRect screenRect = [[lastBrowser screen] visibleFrame];
      NSRect unionRect = NSUnionRect(newBrowserFrame, screenRect);
      if ( !NSEqualRects(unionRect, screenRect) ) {
        topLeft = NSMakePoint(NSMinX(screenRect), NSMaxY(screenRect));
        [[self window] setFrameTopLeftPoint:topLeft];
      }
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

  if ([[[mSidebarTabView selectedTabViewItem] identifier] isEqual:@"historySidebarCHIconTabViewItem"]) {
    [mHistoryDataSource ensureDataSourceLoaded];
    [mHistoryDataSource enableObserver];
  }

  // we used to resize the window here, but we can't if we want any chance of it
  // being allowed to open on the left side. it's too late once we get here.
}

- (void)drawerDidOpen:(NSNotification *)aNotification
{
  // Toggle the sidebar icon.
  if (mSidebarToolbarItem)
    [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarOpened"]];
}

- (void)drawerDidClose:(NSNotification *)aNotification
{
  if ([[[mSidebarTabView selectedTabViewItem] identifier] isEqual:@"historySidebarCHIconTabViewItem"])
    [mHistoryDataSource disableObserver];

  // Unload the Gecko web page in "My Panels" to save memory.
  if(mSidebarToolbarItem)
    [mSidebarToolbarItem setImage:[NSImage imageNamed:@"sidebarClosed"]];

  // restore the frame we cached in |toggleSidebar:|
  if (mDrawerCachedFrame) {
    mDrawerCachedFrame = NO;
    NSRect frame = [[self window] frame];
    if (frame.origin.x == mCachedFrameAfterDrawerOpen.origin.x &&
        frame.origin.y == mCachedFrameAfterDrawerOpen.origin.y &&
        frame.size.width == mCachedFrameAfterDrawerOpen.size.width &&
        frame.size.height == mCachedFrameAfterDrawerOpen.size.height) {

      // Restore the original frame.
      [[self window] setFrame: mCachedFrameBeforeDrawerOpen display: YES animate:NO];	// animation would be nice
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
  else if ( [[item itemIdentifier] isEqual:BookmarkToolbarItemIdentifier] )
    mBookmarkToolbarItem = item;
}

//
// toolbarDidRemoveItem: (toolbar delegate method)
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
  else if ( [[item itemIdentifier] isEqual:ThrobberToolbarItemIdentifier] )
    [self stopThrobber];
  else if ( [[item itemIdentifier] isEqual:BookmarkToolbarItemIdentifier] )
    mBookmarkToolbarItem = nil;
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
                                        ViewSourceToolbarItemIdentifier,
                                        BookmarkToolbarItemIdentifier,
                                        NewTabToolbarItemIdentifier,
                                        CloseTabToolbarItemIdentifier,
                                        TextBiggerToolbarItemIdentifier,
                                        TextSmallerToolbarItemIdentifier,
                                        SendURLToolbarItemIdentifier,
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
    if ( [itemIdent isEqual:BackToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Back", @"Back")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Back", @"Go Back")];
        [toolbarItem setToolTip:NSLocalizedString(@"BackToolTip", @"Go back one page")];
        [toolbarItem setImage:[NSImage imageNamed:@"back"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(back:)];
    }
    else if ( [itemIdent isEqual:ForwardToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Forward", @"Forward")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Forward", @"Go Forward")];
        [toolbarItem setToolTip:NSLocalizedString(@"ForwardToolTip", @"Go forward one page")];
        [toolbarItem setImage:[NSImage imageNamed:@"forward"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(forward:)];
    }
    else if ( [itemIdent isEqual:ReloadToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Reload", @"Reload")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Reload Page", @"Reload Page")];
        [toolbarItem setToolTip:NSLocalizedString(@"ReloadToolTip", @"Reload current page")];
        [toolbarItem setImage:[NSImage imageNamed:@"reload"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(reload:)];
    }
    else if ( [itemIdent isEqual:StopToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Stop", @"Stop")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Stop Loading", @"Stop Loading")];
        [toolbarItem setToolTip:NSLocalizedString(@"StopToolTip", @"Stop loading this page")];
        [toolbarItem setImage:[NSImage imageNamed:@"stop"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(stop:)];
    }
    else if ( [itemIdent isEqual:HomeToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Home", @"Home")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Home", @"Go Home")];
        [toolbarItem setToolTip:NSLocalizedString(@"HomeToolTip", @"Go to home page")];
        [toolbarItem setImage:[NSImage imageNamed:@"home"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(home:)];
    }
    else if ( [itemIdent isEqual:SidebarToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Sidebar", @"Sidebar")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Toggle Sidebar", @"Toggle Sidebar")];
        [toolbarItem setToolTip:NSLocalizedString(@"SidebarToolTip", @"Show or hide the Sidebar")];
        [toolbarItem setImage:[NSImage imageNamed:@"sidebarClosed"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(toggleSidebar:)];
    }
    else if ( [itemIdent isEqual:SearchToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Search", @"Search")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Search", @"Search")];
        [toolbarItem setToolTip:NSLocalizedString(@"SearchToolTip", @"Search the Internet")];
        [toolbarItem setImage:[NSImage imageNamed:@"searchWeb.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(performSearch:)];
    }
    else if ( [itemIdent isEqual:ThrobberToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:@""];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Progress", @"Progress")];
        [toolbarItem setToolTip:NSLocalizedStringFromTable(@"ThrobberPageDefault", @"WebsiteDefaults", nil)];
        [toolbarItem setImage:[NSImage imageNamed:@"throbber-01"]];
        [toolbarItem setTarget:self];
        [toolbarItem setTag:'Thrb'];
        [toolbarItem setAction:@selector(clickThrobber:)];
    }
    else if ( [itemIdent isEqual:LocationToolbarItemIdentifier] )
    {
        NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
        
        [toolbarItem setLabel:NSLocalizedString(@"Location", @"Location")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Location", @"Location")];
        [toolbarItem setView:mLocationToolbarView];
        [toolbarItem setMinSize:NSMakeSize(128,20)];
        [toolbarItem setMaxSize:NSMakeSize(2560,32)];
        
        [menuFormRep setTarget:self];
        [menuFormRep setAction:@selector(performAppropriateLocationAction)];
        [menuFormRep setTitle:[toolbarItem label]];
        
        [toolbarItem setMenuFormRepresentation:menuFormRep];
    }
    else if ( [itemIdent isEqual:PrintToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Print", @"Print")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Print", @"Print")];
        [toolbarItem setToolTip:NSLocalizedString(@"PrintToolTip", @"Print this page")];
        [toolbarItem setImage:[NSImage imageNamed:@"print"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(printDocument:)];
    }
    else if ( [itemIdent isEqual:ViewSourceToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"View Source", @"View Source")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"View Page Source", @"View Page Source")];
        [toolbarItem setToolTip:NSLocalizedString(@"ViewSourceToolTip", @"Display the HTML source of this page")];
        [toolbarItem setImage:[NSImage imageNamed:@"showsource"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(viewSource:)];
    }
    else if ( [itemIdent isEqual:BookmarkToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"Bookmark", @"Bookmark")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"Bookmark Page", @"Bookmark Page")];
        [toolbarItem setToolTip:NSLocalizedString(@"BookmarkToolTip", @"Add this page to your bookmarks")];
        [toolbarItem setImage:[NSImage imageNamed:@"add_to_bookmark.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(bookmarkPage:)];
    }
    else if ( [itemIdent isEqual:TextBiggerToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"BigText", @"Enlarge Text")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"BigText", @"Enlarge Text")];
        [toolbarItem setToolTip:NSLocalizedString(@"BigTextToolTip", @"Enlarge the text on this page")];
        [toolbarItem setImage:[NSImage imageNamed:@"textBigger.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(biggerTextSize:)];
    }
    else if ( [itemIdent isEqual:TextSmallerToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"SmallText", @"Shrink Text")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"SmallText", @"Shrink Text")];
        [toolbarItem setToolTip:NSLocalizedString(@"SmallTextToolTip", @"Shrink the text on this page")];
        [toolbarItem setImage:[NSImage imageNamed:@"textSmaller.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(smallerTextSize:)];
    }
    else if ( [itemIdent isEqual:NewTabToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"NewTab", @"New Tab")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"NewTab", @"New Tab")];
        [toolbarItem setToolTip:NSLocalizedString(@"NewTabToolTip", @"Create a new tab")];
        [toolbarItem setImage:[NSImage imageNamed:@"newTab.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(newTab:)];
    }
    else if ( [itemIdent isEqual:CloseTabToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"CloseTab", @"Close Tab")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"CloseTab", @"Close Tab")];
        [toolbarItem setToolTip:NSLocalizedString(@"CloseTabToolTip", @"Close the current tab")];
        [toolbarItem setImage:[NSImage imageNamed:@"closeTab.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(closeCurrentTab:)];
    }
    else if ( [itemIdent isEqual:SendURLToolbarItemIdentifier] )
    {
        [toolbarItem setLabel:NSLocalizedString(@"SendLink", @"Send Link")];
        [toolbarItem setPaletteLabel:NSLocalizedString(@"SendLink", @"Send Link")];
        [toolbarItem setToolTip:NSLocalizedString(@"SendLinkToolTip", @"Send current URL")];
        [toolbarItem setImage:[NSImage imageNamed:@"sendLink.tif"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(sendURL:)];
    }
    else
    {
        toolbarItem = nil;
    }
    
    return toolbarItem;
}

// This method handles the enabling/disabling of the toolbar buttons.
- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
  // Check the action and see if it matches.
  SEL action = [theItem action];
//  NSLog(@"Validating toolbar item %@ with selector %s", [theItem label], action);
  if (action == @selector(back:))
    return [[mBrowserView getBrowserView] canGoBack];
  else if (action == @selector(forward:))
    return [[mBrowserView getBrowserView] canGoForward];
  else if (action == @selector(reload:))
    return [mBrowserView isBusy] == NO;
  else if (action == @selector(stop:))
    return [mBrowserView isBusy];
  else if (action == @selector(bookmarkPage:))
    return ![mBrowserView isEmpty];
  else if (action == @selector(biggerTextSize:))
    return ![mBrowserView isEmpty] && [[mBrowserView getBrowserView] canMakeTextBigger];
  else if ( action == @selector(smallerTextSize:))
    return ![mBrowserView isEmpty] && [[mBrowserView getBrowserView] canMakeTextSmaller];
  else if (action == @selector(newTab:))
    return [self newTabsAllowed];
  else if (action == @selector(closeCurrentTab:))
    return ([mTabBrowser numberOfTabViewItems] > 1);
  else if (action == @selector(sendURL:))
  {
    NSString* curURL = [[self getBrowserWrapper] getCurrentURLSpec];
    return [curURL length] > 0 && ![curURL isEqualToString:@"about:blank"];
  }
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
  if ( [toolbar isVisible] )
  {
    if ( ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconAndLabel) ||
          ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconOnly) )
    {
      NSArray *itemsWeCanSee = [toolbar visibleItems];
      
      for (unsigned int i = 0; i < [itemsWeCanSee count]; i++)
      {
        if ([[[itemsWeCanSee objectAtIndex:i] itemIdentifier] isEqual:LocationToolbarItemIdentifier])
        {
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
  [[mBrowserView getBrowserView] setActive:NO];
	[mURLBar selectText: self];
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
  [self loadURL:[mLocationSheetURLField stringValue] referrer:nil activate:YES];
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

- (void)cacheBookmarkDS:(BookmarksDataSource*)aDS
{
  mCachedBMDS = aDS;
}

-(IBAction)manageBookmarks: (id)aSender
{
  if ([mSidebarDrawer state] == NSDrawerClosedState)
    [self toggleSidebar: self];

  [mSidebarTabView selectFirstTabViewItem:self];
}

-(IBAction)manageHistory: (id)aSender
{
  if ([mSidebarDrawer state] == NSDrawerClosedState)
    [self toggleSidebar: self];

  [mSidebarTabView selectTabViewItemAtIndex:1];
}

- (void)importBookmarks: (NSString*)aURLSpec
{
  // Open the bookmarks sidebar.
  [self manageBookmarks: self];

  // Now do the importing.
  BrowserWrapper* newView = [[[BrowserWrapper alloc] initWithTab: nil andWindow: [self window]] autorelease];
  [newView setFrame: NSZeroRect];
  [newView setIsBookmarksImport: YES];
  [[[self window] contentView] addSubview: newView];
  [newView loadURI:aURLSpec referrer: nil flags:NSLoadFlagsNone activate:NO];
}

- (IBAction)goToLocationFromToolbarURLField:(id)sender
{
  // trim off any whitespace around url
  NSString *theURL = [[sender stringValue] stringByTrimmingWhitespace];
  
  // look for bookmarks keywords match
  NSArray *resolvedURLs = [[BookmarksManager sharedBookmarksManager] resolveBookmarksKeyword:theURL];

  NSString* resolvedURL = nil;
  if ([resolvedURLs count] == 1)
  {
    resolvedURL = [resolvedURLs lastObject];
    [self loadURL:resolvedURL referrer:nil activate:YES];
  }
  else
  {
  	[self openTabGroup:resolvedURLs replaceExistingTabs:YES];
  }
    
  // global history needs to know the user typed this url so it can present it
  // in autocomplete. We use the URI fixup service to strip whitespace and remove
  // invalid protocols, etc. Don't save keyword-expanded urls.
  if (resolvedURL && [theURL isEqualToString:resolvedURL] && mGlobalHistory && mURIFixer && [theURL length] > 0)
  {
    nsAutoString url;
    [theURL assignTo_nsAString:url];

    nsCOMPtr<nsIURI> fixedURI;
    mURIFixer->CreateFixupURI(url, 0, getter_AddRefs(fixedURI));
    if (fixedURI)
    {
      nsCAutoString spec;
      fixedURI->GetSpec(spec);
      mGlobalHistory->MarkPageAsTyped(spec.get());
    }
  }
}

- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
{
  [[mBrowserView getBrowserView] saveDocument:focusedFrame filterView:aFilterView filterList:aFilterList];
}

- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename
{
  [[mBrowserView getBrowserView] saveURL: aFilterView filterList: aFilterList
                                     url: aURLSpec suggestedFilename: aFilename];
}

- (void)loadSourceOfURL:(NSString*)urlStr
{
  NSString* viewSource = [@"view-source:" stringByAppendingString: urlStr];

  PRBool loadInBackground;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  if (![self newTabsAllowed])
    [self openNewWindowWithURL: viewSource referrer:nil loadInBackground: loadInBackground];
  else
    [self openNewTabWithURL: viewSource referrer:nil loadInBackground: loadInBackground];
}

- (IBAction)viewSource:(id)aSender
{
  NSString* urlStr = [[mBrowserView getBrowserView] getFocusedURLString];
  [self loadSourceOfURL:urlStr];
}

- (IBAction)viewPageSource:(id)aSender
{
  NSString* urlStr = [[mBrowserView getBrowserView] getCurrentURLSpec];
  [self loadSourceOfURL:urlStr];
}

- (IBAction)printDocument:(id)aSender
{
  [[mBrowserView getBrowserView] printDocument];
}

- (IBAction)pageSetup:(id)aSender
{
  [[mBrowserView getBrowserView] pageSetup];
}

- (IBAction)performSearch:(id)aSender
{
  [mBrowserView loadURI:[[PreferenceManager sharedInstance] searchPage] referrer: nil flags:NSLoadFlagsNone activate:NO];
}

- (IBAction)sendURL:(id)aSender
{
  NSString* titleString = nil;
  NSString* urlString = nil;

  [[self getBrowserWrapper] getTitle:&titleString andHref:&urlString];
  
  if (!titleString) titleString = @"";
  if (!urlString)   urlString   = @"";
  
  // we need to encode entities in the title and url strings first. For some reason
  // CFURLCreateStringByAddingPercentEscapes is only happy with UTF-8 strings.
  CFStringRef urlUTF8String   = CFStringCreateWithCString(kCFAllocatorDefault, [urlString   UTF8String], kCFStringEncodingUTF8);
  CFStringRef titleUTF8String = CFStringCreateWithCString(kCFAllocatorDefault, [titleString UTF8String], kCFStringEncodingUTF8);
  
  CFStringRef escapedURL   = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, urlUTF8String,   NULL, CFSTR("&?="), kCFStringEncodingUTF8);
  CFStringRef escapedTitle = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, titleUTF8String, NULL, CFSTR("&?="), kCFStringEncodingUTF8);
    
  NSString* mailtoURLString = [NSString stringWithFormat:@"mailto:?subject=%@&body=%@", (NSString*)escapedTitle, (NSString*)escapedURL];

  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:mailtoURLString]];
  
  CFRelease(urlUTF8String);
  CFRelease(titleUTF8String);
  
  CFRelease(escapedURL);
  CFRelease(escapedTitle);
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
  if (mThrobberImages == nil)
  {
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
    [self loadURL:pageToLoad referrer:nil activate:YES];
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
    useSel = [self bookmarksAreVisible:NO];
  }
  
  [mSidebarBookmarksDataSource addBookmark: self useSelection: useSel isFolder: aIsFolder URL:aURL title:aTitle];
}

- (BOOL)bookmarksAreVisible:(BOOL)inRequireSelection
{
  // we should really identify the tab by identifier, not index.
  BOOL bookmarksShowing =  ([mSidebarDrawer state] == NSDrawerOpenState) && 
            ([mSidebarTabView tabViewItemAtIndex: 0] == [mSidebarTabView selectedTabViewItem]);
            
  if (inRequireSelection)
    bookmarksShowing &= ([mSidebarBookmarksDataSource haveSelectedRow]);
  
  return bookmarksShowing;
}

- (IBAction)bookmarkPage: (id)aSender
{
  [self addBookmarkExtended:YES isFolder:NO URL:nil title:nil];
}


- (IBAction)bookmarkLink: (id)aSender
{
  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  GeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);
  nsAutoString linkText;
  GeckoUtils::GatherTextUnder(linkContent, linkText);
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
  unsigned int reloadFlags = NSLoadFlagsNone;
  
  if (([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0)
    reloadFlags = NSLoadFlagsBypassCacheAndProxy;
  
  [[mBrowserView getBrowserView] reload: reloadFlags];
}

- (IBAction)stop:(id)aSender
{
  [[mBrowserView getBrowserView] stop: nsIWebNavigation::STOP_ALL];
}

- (IBAction)home:(id)aSender
{
  [mBrowserView loadURI:[[PreferenceManager sharedInstance] homePage:NO] referrer: nil flags:NSLoadFlagsNone activate:NO];
}

- (NSString*)getContextMenuNodeDocumentURL
{
  if (!mContextMenuNode) return @"";
  
  nsCOMPtr<nsIDOMDocument> ownerDoc;
  mContextMenuNode->GetOwnerDocument(getter_AddRefs(ownerDoc));

  nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(ownerDoc);
  if (!nsDoc) return @"";

  nsCOMPtr<nsIDOMLocation> location;
  nsDoc->GetLocation(getter_AddRefs(location));
  if (!location) return @"";
	
  nsAutoString urlStr;
  location->GetHref(urlStr);
  return [NSString stringWith_nsAString:urlStr];
}

- (IBAction)frameToNewWindow:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self openNewWindowWithURL:frameURL referrer:nil loadInBackground:NO];		// follow the pref?
}

- (IBAction)frameToNewTab:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self openNewTabWithURL:frameURL referrer:nil loadInBackground:NO];		// follow the pref?
}

- (IBAction)frameToThisWindow:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self loadURL:frameURL referrer:nil activate:YES];
}


- (IBAction)toggleSidebar:(id)aSender
{
  // Force the window to shrink and move if necessary in order to accommodate the sidebar. We check
  // if it will fit on either the left or on the right, and if it won't, shrink the window. We 
  // used to do this in |drawerWillOpen:| but the problem is that as soon as wel tell cocoa to 
  // toggle, it makes up its mind about what side it's going to open on. On multiple monitors, 
  // if the window was on the secondary this could end up having the sidebar across the fold of
  // the monitors. By placing the code here, we guarantee that if we are going to resize the
  // window, we leave space for the drawer on the right BEFORE it starts, and cocoa will put it there.
  
  NSRect screenFrame = [[[self window] screen] visibleFrame];
  NSRect windowFrame = [[self window] frame];
  NSSize drawerSize = [mSidebarDrawer contentSize];
  int fudgeFactor = 12; // Not sure how to get the drawer's border info, so we fudge it for now.
  drawerSize.width += fudgeFactor;

  // check that opening on the right won't flow off the edge of the screen on the right AND
  // opening on the left won't flow off the edge of the screen on the left. If both are true,
  // we have to resize the window.
  if (windowFrame.origin.x + windowFrame.size.width + drawerSize.width > screenFrame.origin.x + screenFrame.size.width &&
        windowFrame.origin.x - drawerSize.width < screenFrame.origin.x) {
    // We need to adjust the window so that it can fit.
    float shrinkDelta = (windowFrame.size.width + drawerSize.width) - screenFrame.size.width;
    if (shrinkDelta < 0) shrinkDelta = 0;
    float newWidth = (windowFrame.size.width - shrinkDelta);
    float newPosition = screenFrame.size.width - newWidth - drawerSize.width + screenFrame.origin.x;
    if (newPosition < screenFrame.origin.x) newPosition = screenFrame.origin.x;
    mCachedFrameBeforeDrawerOpen = windowFrame;
    windowFrame.origin.x = newPosition;
    windowFrame.size.width = newWidth;
    mCachedFrameAfterDrawerOpen = windowFrame;
    [[self window] setFrame: windowFrame display: YES animate:NO];		// animation  would be nice, but is too slow
    mDrawerCachedFrame = YES;
  }

  [mSidebarDrawer toggle:aSender];
}

// map command-left arrow to 'back'
- (void)moveToBeginningOfLine:(id)sender
{
  [self back:sender];
}

// map command-right arrow to 'forward'
- (void)moveToEndOfLine:(id)sender
{
  [self forward:sender];
}

// map delete key to Back
- (void)deleteBackward:(id)sender
{
  if ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask)
    [self forward:sender];
  else
    [self back:sender];
}

-(void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer activate:(BOOL)activate
{
    if (mInitialized) {
      [mBrowserView loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone activate:activate];
    }
    else {
        // we haven't yet initialized all the browser machinery, stash the url and referrer
        // until we're ready in windowDidLoad:
        mPendingURL = aURLSpec;
        [mPendingURL retain];
        mPendingReferrer = aReferrer;
        [mPendingReferrer retain];
        mPendingActivate = activate;
    }
}

- (void)updateLocationFields:(NSString *)locationString
{
    if ( [locationString isEqual:@"about:blank"] )
      locationString = @"";

    // if the urlbar has focus (actually if its field editor has focus), we
    // need to use one of its routines to update the autocomplete status or
    // we could find ourselves with stale results and the popup still open. If
    // it doesn't have focus, we can bypass all that and just use normal routines.
    if ( [[self window] firstResponder] == [mURLBar fieldEditor] )
      [mURLBar setStringUndoably:locationString fromLocation:0];		// updates autocomplete correctly
    else
      [mURLBar setStringValue:locationString];
    [mLocationSheetURLField setStringValue:locationString];

    // don't call [window display] here, no matter how much you might want
    // to, because it forces a redraw of every view in the window and with a lot
    // of tabs, it's dog slow.
    // [[self window] display];
}

- (void)updateSiteIcons:(NSImage *)siteIconImage
{
  if (siteIconImage == nil)
    siteIconImage = [NSImage imageNamed:@"globe_ico"];
	[mProxyIcon setImage:siteIconImage];
}

//
// closeBrowserWindow:
//
// Some gecko view in this window wants to close the window. If we have
// a bunch of tabs, only close the relevant tab, otherwise close the
// window as a whole.
//
- (void)closeBrowserWindow:(BrowserWrapper*)inBrowser
{
  if ( [mTabBrowser numberOfTabViewItems] > 1 ) {
    // close the appropriate browser (it may not be frontmost) and
    // remove it from the tab UI
    [inBrowser windowClosed];
    [mTabBrowser removeTabViewItem:[inBrowser tab]];
  }
  else
  {
    // if a window unload handler calls window.close(), we
    // can get here while the window is already being closed,
    // so we don't want to close it again (and recurse).
    if (!mClosingWindow)
      [[self window] close];
  }
}

- (void)createNewTab:(ENewTabContents)contents;
{
    BrowserTabViewItem* newTab  = [self createNewTabItem];
    BrowserWrapper*     newView = [newTab view];
    
    BOOL loadHomepage = NO;
    if (contents == eNewTabHomepage)
    {
      nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
      PRInt32 newTabPage = 0;		// 0=about:blank, 1=home page, 2=last page visited
      pref->GetIntPref("browser.tabs.startPage", &newTabPage);
      loadHomepage = (newTabPage == 1);
    }

    [newTab setLabel: (loadHomepage ? NSLocalizedString(@"TabLoading", @"") : NSLocalizedString(@"UntitledPageTitle", @""))];
    [mTabBrowser addTabViewItem: newTab];
    
    BOOL focusURLBar = NO;
    if (contents != eNewTabEmpty)
    {
      // Focus the URL bar if we're opening a blank tab and the URL bar is visible.
      NSToolbar* toolbar = [[self window] toolbar];
      BOOL locationBarVisible = [toolbar isVisible] &&
                                ([toolbar displayMode] == NSToolbarDisplayModeIconAndLabel ||
                                 [toolbar displayMode] == NSToolbarDisplayModeIconOnly);
                                
      NSString* urlToLoad = @"about:blank";
      if (loadHomepage)
        urlToLoad = [[PreferenceManager sharedInstance] homePage: NO];

      focusURLBar = locationBarVisible && [urlToLoad isEqualToString:@"about:blank"];      

      [newView loadURI:urlToLoad referrer:nil flags:NSLoadFlagsNone activate:!focusURLBar];
    }
    
    [mTabBrowser selectLastTabViewItem: self];
    
    if (focusURLBar)
      [self focusURLBar];
}

- (IBAction)newTab:(id)sender
{
  [self createNewTab:eNewTabHomepage];  // we'll look at the pref to decide whether to load the home page
}

-(IBAction)closeCurrentTab:(id)sender
{
  if ( [mTabBrowser numberOfTabViewItems] > 1 ) {
    [[[mTabBrowser selectedTabViewItem] view] windowClosed];
    [mTabBrowser removeTabViewItem:[mTabBrowser selectedTabViewItem]];
  }
}

- (IBAction)previousTab:(id)sender
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == 0)
    [mTabBrowser selectLastTabViewItem:sender];
  else
    [mTabBrowser selectPreviousTabViewItem:sender];
}

- (IBAction)nextTab:(id)sender
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == [mTabBrowser numberOfTabViewItems] - 1)
    [mTabBrowser selectFirstTabViewItem:sender];
  else
    [mTabBrowser selectNextTabViewItem:sender];
}

- (IBAction)closeSendersTab:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      [[tabViewItem view] windowClosed];
      [mTabBrowser removeTabViewItem:tabViewItem];
    }
  }
}

- (IBAction)closeOtherTabs:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      while ([mTabBrowser numberOfTabViewItems] > 1)
      {
        NSTabViewItem* doomedItem = nil;
        if ([mTabBrowser indexOfTabViewItem:tabViewItem] == 0)
          doomedItem = [mTabBrowser tabViewItemAtIndex:1];
        else
          doomedItem = [mTabBrowser tabViewItemAtIndex:0];
        
        [[doomedItem view] windowClosed];
        [mTabBrowser removeTabViewItem:doomedItem];
      }
    }
  }
}

- (IBAction)reloadSendersTab:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      [[[tabViewItem view] getBrowserView] reload: NSLoadFlagsNone];
    }
  }
}

- (IBAction)moveTabToNewWindow:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      NSString* url = [[tabViewItem view] getCurrentURLSpec];

      PRBool backgroundLoad = PR_FALSE;
      nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
      if (pref)
        pref->GetBoolPref("browser.tabs.loadInBackground", &backgroundLoad);

      [self openNewWindowWithURL:url referrer:nil loadInBackground:backgroundLoad];

      [[tabViewItem view] windowClosed];
      [mTabBrowser removeTabViewItem:tabViewItem];
    }
  }
}

- (void)tabView:(NSTabView *)tabView willSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
  // we'll get called for browser tab views as well. ignore any calls coming from
  // there, we're only interested in the sidebar.
  if (tabView != mSidebarTabView)
    return;

  if ([[tabViewItem identifier] isEqual:@"historySidebarCHIconTabViewItem"]) {
    [mHistoryDataSource ensureDataSourceLoaded];
    [mHistoryDataSource enableObserver];
  }
  else
    [mHistoryDataSource disableObserver];
}

- (void)tabView:(NSTabView *)aTabView didSelectTabViewItem:(NSTabViewItem *)aTabViewItem
{
  // we'll get called for the sidebar tabs as well. ignore any calls coming from
  // there, we're only interested in the browser tabs.
  if (aTabView != mTabBrowser)
    return;

  // Disconnect the old view, if one has been designated.
  // If the window has just been opened, none has been.
  if ( mBrowserView )
    [mBrowserView disconnectView];

  // Connect up the new view
  mBrowserView = [aTabViewItem view];
      
  // Make the new view the primary content area.
  [mBrowserView makePrimaryBrowserView: mURLBar status: mStatus windowController: self];
}

- (void)tabViewDidChangeNumberOfTabViewItems:(NSTabView *)aTabView
{
  [[NSApp delegate] fixCloseMenuItemKeyEquivalents];
}

-(id)getTabBrowser
{
  return mTabBrowser;
}

- (BOOL)newTabsAllowed
{
  return [mTabBrowser canMakeNewTabs];
}

-(BrowserWrapper*)getBrowserWrapper
{
  return mBrowserView;
}

-(void)openNewWindowWithURL: (NSString*)aURLSpec referrer: (NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];  
  [browser loadURL: aURLSpec referrer:aReferrer activate:!aLoadInBG];
  
  if (aLoadInBG)
  {
    [[browser window] setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [[browser window] orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
    [[browser window] setSuppressMakeKeyFront:NO];
  }
  else
    [browser showWindow:self];
}

- (void)openNewWindowWithGroup: (nsIContent*)aFolderContent loadInBackground: (BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  // Tell the Tab Browser in the newly created window to load the group
  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  if (aLoadInBG)
  {
    [[browser window] setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [[browser window] orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
    [[browser window] setSuppressMakeKeyFront:NO];
  }
  else
    [browser showWindow:self];

  BookmarksManager* bmManager = [BookmarksManager sharedBookmarksManager];
  BookmarkItem*     item			= [bmManager getWrapperForContent:aFolderContent];

  NSArray* groupURLs = [bmManager getBookmarkGroupURIs:item];
	[browser openTabGroup:groupURLs replaceExistingTabs:YES];
}

-(void)openNewTabWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG
{
  BrowserTabViewItem* newTab  = [self createNewTabItem];
  BrowserWrapper*     newView = [newTab view];

  // hyatt originally made new tabs open on the far right and tabs opened from a content
  // link open to the right of the current tab. The idea was to keep the new tab
  // close to the tab that spawned it, since they are related. Users, however, got confused
  // as to why tabs appeared in different places, so now all tabs go on the far right.
#ifdef OPEN_TAB_TO_RIGHT_OF_SELECTED    
  NSTabViewItem* selectedTab = [mTabBrowser selectedTabViewItem];
  int index = [mTabBrowser indexOfTabViewItem: selectedTab];
  [mTabBrowser insertTabViewItem: newTab atIndex: index+1];
#else
  [mTabBrowser addTabViewItem: newTab];
#endif

  [newTab setLabel: NSLocalizedString(@"TabLoading", @"")];

  [newView loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone activate:!aLoadInBG];

  if (!aLoadInBG)
    [mTabBrowser selectTabViewItem: newTab];
}

- (void)openTabGroup:(NSArray*)urlArray replaceExistingTabs:(BOOL)replaceExisting
{
  int curNumTabs	= [mTabBrowser numberOfTabViewItems];
  int numItems 		= (int)[urlArray count];
  
  for (int i = 0; i < numItems; i++)
  {
    NSString* thisURL = [urlArray objectAtIndex:i];
    BrowserTabViewItem* tabViewItem;

    if (replaceExisting && i < curNumTabs)
    {
      tabViewItem = [mTabBrowser tabViewItemAtIndex:i];
    }
    else
    {
      tabViewItem = [self createNewTabItem];
      [tabViewItem setLabel: NSLocalizedString(@"UntitledPageTitle", @"")];
      [mTabBrowser addTabViewItem: tabViewItem];
    }

    [[tabViewItem view] loadURI: thisURL referrer:nil
                        flags: NSLoadFlagsNone activate:(i == 0)];
                        
    if (![mTabBrowser canMakeNewTabs])
      break;		// we'll throw away the rest of the items. Too bad.
  }
 
  // Select the first tab.
  [mTabBrowser selectTabViewItemAtIndex:replaceExisting ? 0 : curNumTabs];
}

-(BrowserTabViewItem*)createNewTabItem
{
  BrowserTabViewItem* newTab = [BrowserTabView makeNewTabItem];
  BrowserWrapper* newView = [[[BrowserWrapper alloc] initWithTab: newTab andWindow: [mTabBrowser window]] autorelease];

  [newTab setView: newView];
  
  // we have to copy the context menu for each tag, because
  // the menu gets the tab view item's tag.
  NSMenu* contextMenu = [mTabMenu copy];
  [[newTab tabItemContentsView] setMenu:contextMenu];
  [contextMenu release];

  return newTab;
}

-(void)setupSidebarTabs
{
    IconTabViewItem   *bookItem = [[[IconTabViewItem alloc] initWithIdentifier:@"bookmarkSidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"bookicon"]] autorelease];
    IconTabViewItem   *histItem = [[[IconTabViewItem alloc] initWithIdentifier:@"historySidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"historyicon"]] autorelease];
#if USE_SEARCH_ITEM
    IconTabViewItem *searchItem = [[[IconTabViewItem alloc] initWithIdentifier:@"searchSidebarCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"searchicon"]] autorelease];
#endif
#if USE_PANELS_ITEM
    IconTabViewItem *panelsItem = [[[IconTabViewItem alloc] initWithIdentifier:@"myPanelsCHIconTabViewItem"
                                  withTabIcon:[NSImage imageNamed:@"panel_icon"]] autorelease];
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
    
#if USE_PREF_FOR_HISTORY_PANEL
    BOOL showHistory = NO;
    nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
    if (pref) {
      PRBool historyPref = PR_FALSE;
      if (NS_SUCCEEDED(pref->GetBoolPref("chimera.show_history", &historyPref)))
        showHistory = historyPref ? YES : NO;
    }    
    if (!showHistory)
      [mSidebarTabView removeTabViewItem:[mSidebarTabView tabViewItemAtIndex:1]];
#endif
    
    [mSidebarTabView setDelegate:self];
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

- (IBAction)biggerTextSize:(id)aSender
{
  [[mBrowserView getBrowserView] biggerTextSize];
}

- (IBAction)smallerTextSize:(id)aSender
{
  [[mBrowserView getBrowserView] smallerTextSize];
}

- (void)getInfo:(id)sender
{
  [mSidebarBookmarksDataSource ensureBookmarks];
  [mSidebarBookmarksDataSource showBookmarkInfo:sender];
}

- (BOOL)canGetInfo
{
  return [self bookmarksAreVisible:YES];
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
  BOOL showFrameItems = NO;
  
  NSMenu* menuPrototype = nil;
  if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_LINK) != 0)
  {
    if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0)
      menuPrototype = mImageLinkMenu;
    else
      menuPrototype = mLinkMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_INPUT) != 0 ||
           (mContextMenuFlags & nsIContextMenuListener::CONTEXT_TEXT) != 0)
  {
    menuPrototype = mInputMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0)
  {
    menuPrototype = mImageMenu;
  }
  else if ((mContextMenuFlags & nsIContextMenuListener::CONTEXT_DOCUMENT) != 0)
  {
    menuPrototype = mPageMenu;
    [mBackItem 		setEnabled: [[mBrowserView getBrowserView] canGoBack]];
    [mForwardItem setEnabled: [[mBrowserView getBrowserView] canGoForward]];
    [mCopyItem		setEnabled: [[mBrowserView getBrowserView] canCopy]];
  }
  
  if (mContextMenuNode)
  {
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    mContextMenuNode->GetOwnerDocument(getter_AddRefs(ownerDoc));
  
    nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([[mBrowserView getBrowserView] getContentWindow]);

    nsCOMPtr<nsIDOMDocument> contentDoc;
    if (contentWindow)
      contentWindow->GetDocument(getter_AddRefs(contentDoc));
    
    showFrameItems = (contentDoc != ownerDoc);
  }

  // we have to clone the menu and return that, so that we don't change
  // our only copy of the menu
  NSMenu* result = [[menuPrototype copy] autorelease];

  const int kFrameRelatedItemsTag 			= 100;
  const int kFrameInapplicableItemsTag 	= 101;
  
  if (showFrameItems)
  {
    NSMenuItem* frameItem;
    while ((frameItem = [result itemWithTag:kFrameInapplicableItemsTag]) != nil)
      [result removeItem:frameItem];
  }
  else
  {
    NSMenuItem* frameItem;
    while ((frameItem = [result itemWithTag:kFrameRelatedItemsTag]) != nil)
      [result removeItem:frameItem];
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
  GeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);

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

  if (aUseWindow || ![self newTabsAllowed])
    [self openNewWindowWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
  else
    [self openNewTabWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground];
}

- (IBAction)savePageAs:(id)aSender
{
  [self saveDocument:NO filterView:nil filterList: nil];
}

- (IBAction)saveFrameAs:(id)aSender
{
  [self saveDocument:YES filterView:nil filterList: nil];
}

- (IBAction)saveLinkAs:(id)aSender
{
  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  GeckoUtils::GetEnclosingLinkElementAndHref(mContextMenuNode, getter_AddRefs(linkContent), href);

  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return;

  NSString* hrefStr = [NSString stringWith_nsAString: href];

  // The user wants to save this link.
  nsAutoString text;
  GeckoUtils::GatherTextUnder(mContextMenuNode, text);

  [self saveURL: nil filterList: nil
            url: hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
}

- (IBAction)saveImageAs:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mContextMenuNode));
  if (imgElement)
  {
      nsAutoString text;
      imgElement->GetAttribute(NS_LITERAL_STRING("src"), text);
      nsAutoString url;
      imgElement->GetSrc(url);

      NSString* hrefStr = [NSString stringWith_nsAString: url];

      [self saveURL: nil filterList: nil
                url: hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
  }
}

- (IBAction)copyImageLocation:(id)sender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(webBrowser));
  if (clipboard)
    clipboard->CopyImageLocation();
}

- (IBAction)copyLinkLocation:(id)aSender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
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

    [self loadURL: urlStr referrer:referrer activate:YES];
  }  
}

- (BookmarksToolbar*) bookmarksToolbar
{
  return mPersonalToolbar;
}

- (NSProgressIndicator*)progressIndicator
{
  return mProgress;
}

- (void)showProgressIndicator
{
  // note we do nothing to check if the progress indicator is already there.
  [mProgressSuperview addSubview:mProgress];
}

- (void)hideProgressIndicator
{
  [mProgress removeFromSuperview];
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

- (NSDrawer *)sidebarDrawer
{
    return mSidebarDrawer;
}

- (PageProxyIcon *)proxyIconView
{
  return mProxyIcon;
}

- (BookmarksDataSource*)bookmarksDataSource
{
  return mSidebarBookmarksDataSource;
}

- (id)windowWillReturnFieldEditor:(NSWindow *)aWindow toObject:(id)anObject
{
  if ([anObject isEqual:mURLBar]) {
    if (!mURLFieldEditor) {
      mURLFieldEditor = [[AutoCompleteTextFieldEditor alloc] init];
      [mURLFieldEditor setFieldEditor:YES];
      [mURLFieldEditor setAllowsUndo:YES];
    }
    return mURLFieldEditor;
  }
  return nil;
}

@end

#pragma mark -

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
  [super dealloc];
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

