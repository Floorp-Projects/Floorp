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
#import "BrowserWindow.h"

#import "BookmarkToolbar.h"
#import "BookmarkViewController.h"
#import "BookmarkManager.h"

#import "BrowserContentViews.h"
#import "BrowserWrapper.h"
#import "PreferenceManager.h"
#import "HistoryDataSource.h"
#import "BrowserTabView.h"
#import "UserDefaults.h"
#import "PageProxyIcon.h"
#import "AutoCompleteTextField.h"
#import "SearchTextField.h"
#import "SearchTextFieldCell.h"
#import "STFPopUpButtonCell.h"
#import "MainController.h"
#import "DraggableImageAndTextCell.h"

#include "nsIWebNavigation.h"
#include "nsISHistory.h"
#include "nsIHistoryEntry.h"
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
#include "nsNetUtil.h"

#include "nsIClipboardCommands.h"
#include "nsIWebBrowser.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManagerUtils.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIBrowserHistory.h"
#include "nsIPermissionManager.h"
#include "nsIWebPageDescriptor.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFocusController.h"

#include <QuickTime/QuickTime.h>

#define USE_DRAWER_FOR_BOOKMARKS 0
#if USE_DRAWER_FOR_BOOKMARKS
#import "ImageAndTextCell.h"
#endif

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
  NSFont* mDefaultFont;	// will be needed if editing empty field
  NSUndoManager *mUndoManager; //we handle our own undo to avoid stomping on bookmark undo
}
- (id)initWithFrame:(NSRect)bounds defaultFont:(NSFont*)defaultFont;
@end

@implementation AutoCompleteTextFieldEditor

// sets the defaultFont so that we don't paste in the wrong one
- (id)initWithFrame:(NSRect)bounds defaultFont:(NSFont*)defaultFont
{
  if ((self = [super initWithFrame:bounds])) {
    mDefaultFont = defaultFont;
    mUndoManager = [[NSUndoManager alloc] init];
    [self setDelegate:self];
  }
  return self;
}

-(void) dealloc
{
  [mUndoManager release];
  [super dealloc];
}

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
        if (NSMaxRange(aRange) == 0 && mDefaultFont) // will only be true if the field is empty
          [self setFont:mDefaultFont];	// wrong font will be used otherwise
        [self didChangeText];
      }
      // after a paste, the insertion point should be after the pasted text
      unsigned int newInsertionPoint = aRange.location + [newText length];
      [self setSelectedRange:NSMakeRange(newInsertionPoint,0)];
      return;
    }
  }
}

- (NSUndoManager *)undoManagerForTextView:(NSTextView *)aTextView
{
  if (aTextView == self)
    return mUndoManager;
  return nil;
}

@end
//////////////////////////////////////

#pragma mark -

//
// IconPopUpCell
//
// A popup cell that displays only an icon with no border, yet retains the
// behaviors of a popup menu. It's amazing you can't get this w/out having
// to subclass, but *shrug*.
//
@interface IconPopUpCell : NSPopUpButtonCell
{
@private
  NSImage* fImage;
  NSRect fSrcRect;      // rect cached for drawing, same size as image
}
- (id)initWithImage:(NSImage *)inImage;
@end

@implementation IconPopUpCell

- (id)initWithImage:(NSImage *)inImage
{
  if ( (self = [super initTextCell:@"" pullsDown:YES]) )
  {
    fImage = [inImage retain];
    fSrcRect = NSMakeRect(0,0,0,0);
    fSrcRect.size = [fImage size];
  }
  return self;
}

- (void)dealloc
{
  [fImage release];
  [super dealloc];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  [fImage setFlipped:[controlView isFlipped]];
  cellFrame.size = fSrcRect.size;                  // don't scale
  [fImage drawInRect:cellFrame fromRect:fSrcRect operation:NSCompositeSourceOver fraction:1.0];
}

@end

#pragma mark -

//
// interface ToolbarViewItem
//
// NSToolbarItem, by default, doesn't do validation for view items. Override
// that behavior to call |-validateToolbarItem:| on the item's target.
//
@interface ToolbarViewItem : NSToolbarItem
{
}
@end

@implementation ToolbarViewItem

//
// -validate
//
// Override default behavior (which does nothing at all for a view item) to
// ask the target to handle it. The target must perform all the appropriate
// enabling/disabling within |-validateToolbarItem:| because we can't know
// all the details. The return value is ignored.
//
- (void)validate
{
  id target = [self target];
  if ([target respondsToSelector:@selector(validateToolbarItem:)])
    [target validateToolbarItem:self];
}

@end

#pragma mark -

//
// interface ToolbarButton
//
// A subclass of NSButton that responds to |-setControlSize:| which
// comes from the toolbar when it changes sizes. Adjust the size
// of our associated NSToolbarItem when the call comes.
//
// Note that |-setControlSize:| is not part of NSView's api, but the
// toolbar code calls it anyway, without any documentation to that
// effect.
//
@interface ToolbarButton : NSButton
{
  NSToolbarItem* mToolbarItem;
}
-(id)initWithFrame:(NSRect)inFrame item:(NSToolbarItem*)inItem;
@end

@implementation ToolbarButton

-(id)initWithFrame:(NSRect)inFrame item:(NSToolbarItem*)inItem
{
  if ((self = [super initWithFrame:inFrame])) {
    mToolbarItem = inItem;
  }
  return self;
}

//
// -setControlSize:
//
// Called by the toolbar when the toolbar changes icon size. Adjust our
// toolbar item so that it can adjust larger or smaller.
//
- (void)setControlSize:(NSControlSize)size
{
  NSSize s;
  if (size == NSRegularControlSize) {
    s = NSMakeSize(32., 32.);
    [mToolbarItem setMinSize:s];
    [mToolbarItem setMaxSize:s];
  }
  else {
    s = NSMakeSize(24., 24.);
    [mToolbarItem setMinSize:s];
    [mToolbarItem setMaxSize:s];
  }
  [[self image] setSize:s];
}

//
// -controlSize
//
// The toolbar assumes this implemented whenever |-setControlSize:| is implemented,
// though I'm not sure why. 
//
- (NSControlSize)controlSize
{
  return [[self cell] controlSize];
}

@end

#pragma mark -

@interface BrowserWindowController(Private)
  // open a new window or tab, but doesn't load anything into them. Must be matched
  // with a call to do that.
- (BrowserWindowController*)openNewWindow:(BOOL)aLoadInBG;
- (BrowserTabViewItem*)openNewTab:(BOOL)aLoadInBG;

- (void)setupToolbar;
- (void)setupSidebarTabs;
- (NSString*)getContextMenuNodeDocumentURL;
- (void)loadSourceOfURL:(NSString*)urlStr;
- (void) transformFormatString:(NSMutableString*)inFormat domain:(NSString*)inDomain search:(NSString*)inSearch;
-(void)openNewWindowWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
-(void)openNewTabWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
- (BOOL)isPageTextFieldFocused;

// create back/forward session history menus on toolbar button
- (IBAction)backMenu:(id)inSender;
- (IBAction)forwardMenu:(id)inSender;
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
    mSavedTitle = nil;
    mBookmarkViewControllerInitialized = NO;
  
    // register for services
    NSArray* sendTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    NSArray* returnTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    [NSApp registerServicesMenuSendTypes:sendTypes returnTypes:returnTypes];
    
    nsCOMPtr<nsIBrowserHistory> globalHist = do_GetService("@mozilla.org/browser/global-history;2");
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
  [self autosaveWindowFrame];
  
  // ensure that the URL auto-complete popup is closed before the mork
  // database is shut down, or we crash
  [mURLBar clearResults];

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
  
  // if the bookmark manager is visible when we close the window, all hell
  // breaks loose in the autorelease pool and when we try to show another
  // window. Honestly, I don't know why, but the easy fix is to simply
  // ensure that the browser is visible when we close.
  [self ensureBrowserVisible:self];
  
  // we have to manually enable/disable the bookmarks menu items, because we
  // turn autoenabling off for that menu
  [[NSApp delegate] adjustBookmarksMenuItemsEnabling:NO];

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // autorelease just in case we're here because of a window closing
  // initiated from gecko, in which case this BWC would still be on the 
  // stack and may need to stay alive until it unwinds. We've already
  // shut down gecko above, so we can safely go away at a later time.
  [self autorelease];
}

//
// - stopAllPendingLoads
//
// For each tab, stop it from loading
//
- (void)stopAllPendingLoads
{
  int numTabs = [mTabBrowser numberOfTabViewItems];
  for (int i = 0; i < numTabs; i++) {
    NSTabViewItem* item = [mTabBrowser tabViewItemAtIndex: i];
    [[[item view] getBrowserView] stop: nsIWebNavigation::STOP_ALL];
  }
}

- (void)dealloc
{
#if DEBUG
  NSLog(@"Browser controller died.");
#endif

  // clear the window-level undo manager used by the edit field. Not sure
  // why this isn't automatically done, but we'll leave objects hanging around in
  // the undo/redo if we do not. We also cannot do this in the url bar's dealloc, 
  // it only works if it's here.
  [[[self window] undoManager] removeAllActions];

  // active Gecko connections have already been shut down in |windowWillClose|
  // so we don't need to worry about that here. We only have to be careful
  // not to access anything related to the document, as it's been destroyed. The
  // superclass dealloc takes care of our child NSView's, which include the 
  // BrowserWrappers and their child CHBrowserViews.
  
  //if (mSidebarBrowserView)
  //  [mSidebarBrowserView windowClosed];

  [mSavedTitle release];
  [mProgress release];
  [mPopupBlocked release];
  [mSearchBar release];
  [self stopThrobber];
  [mThrobberImages release];
  [mURLFieldEditor release];
  
  [super dealloc];
}

//
// windowDidLoad
//
// setup all the things we can't do in the nib. Note that we defer the setup of
// the bookmarks view until the user actually displays it the first time.
//
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
      mPopupBlocked = nil;
    }
    else {
      // Retain with a single extra refcount. This allows us to remove
      // the progress meter from its superview without having to worry
      // about retaining and releasing it. Cache the superview of the
      // progress. Dynamically fetch the superview so as not to burden
      // someone rearranging the nib with this detail. Note that this
      // needs to be in a subview from the status bar because if the
      // window resizes while it is hidden, its position wouldn't get updated.
      // Having it in a separate view that stays visible (and is thus
      // involved in the layout process) solves this.
      [mProgress retain];
      mProgressSuperview = [mProgress superview];
      
      // due to a cocoa issue with it updating the bounding box of two rects
      // that both needing updating instead of just the two individual rects
      // (radar 2194819), we need to make the text area opaque.
      [mStatus setBackgroundColor:[NSColor windowBackgroundColor]];
      [mStatus setDrawsBackground:YES];
      
      // create a new cell for our popup blocker item that draws just an image
      // yet still retains the functionality of a popdown menu. Like the progress
      // meter above, we retain so we can hide with impunity and grab its superview.
      // However, unlike the progress meter, this doesn't need to be in a subview from
      // the status bar because it is in a fixed position on the LHS.
      [mPopupBlocked retain];
      NSFont* savedFont = [[mPopupBlocked cell] font];
      NSMenu* savedMenu = [mPopupBlocked menu];     // must cache this before replacing cell
      IconPopUpCell* iconCell = [[[IconPopUpCell alloc] initWithImage:[NSImage imageNamed:@"popup-blocked"]] autorelease];
      [mPopupBlocked setCell:iconCell];
      [iconCell setFont:savedFont];
      [mPopupBlocked setToolTip:NSLocalizedString(@"A web popup was blocked", "Web Popup Toolitp")]; 
//      [iconCell setPreferredEdge:NSMaxYEdge];
      [iconCell setMenu:savedMenu];
      [iconCell setBordered:NO];
      mPopupBlockSuperview = [mPopupBlocked superview];
      [self showPopupBlocked:NO];
      
      // register for notifications so we can populate the popup blocker menu
      // right before it's displayed.
      [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(buildPopupBlockerMenu:)
                                              name:NSPopUpButtonCellWillPopUpNotification object:iconCell];
    }

    // Set up the toolbar's search text field
    NSMutableArray *searchTitles =
      [NSMutableArray arrayWithArray:[[[BrowserWindowController searchURLDictionary] allKeys] sortedArrayUsingSelector:@selector(compare:)]];

    [searchTitles removeObject:@"PreferredSearchEngine"];

    [mSearchBar addPopUpMenuItemsWithTitles:searchTitles];
    [[[mSearchBar cell] popUpButtonCell] selectItemWithTitle:
      [[BrowserWindowController searchURLDictionary] objectForKey:@"PreferredSearchEngine"]];

    [mSearchBar retain];
    [mSearchBar removeFromSuperview];

    // Set the sheet's search text field
    [mSearchSheetTextField addPopUpMenuItemsWithTitles:searchTitles];
    [[[mSearchSheetTextField cell] popUpButtonCell] selectItemWithTitle:
      [[BrowserWindowController searchURLDictionary] objectForKey:@"PreferredSearchEngine"]];    
    
    // Get our saved dimensions.
    NSRect oldFrame = [[self window] frame];
    [[self window] setFrameUsingName: NavigatorWindowFrameSaveName];
    
    if (NSEqualSizes(oldFrame.size, [[self window] frame].size))
      mustResizeChrome = YES;
    
    mInitialized = YES;
        
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
      [mPersonalToolbar buildButtonList];
    
      if (![self shouldShowBookmarkToolbar])
        [mPersonalToolbar showBookmarksToolbar:NO];
    }
    
    if (mustResizeChrome)
      [mContentView resizeSubviewsWithOldSize:[mContentView frame].size];
      
    // stagger window from last browser, if there is one. we can't just use autoposition
    // because it doesn't work on multiple monitors (radar bug 2972893). |getFrontmostBrowserWindow|
    // only gets fully chromed windows, so this will do the right thing for popups (yay!).
    const int kWindowStaggerOffset = 15;
    NSWindow* lastBrowser = [[NSApp delegate] getFrontmostBrowserWindow];
    if ( lastBrowser != [self window] ) {
      NSRect screenRect = [[lastBrowser screen] visibleFrame];
      NSRect testBrowserFrame = [lastBrowser frame];
      NSPoint previousOrigin = testBrowserFrame.origin;
      testBrowserFrame.origin.x += kWindowStaggerOffset;
      testBrowserFrame.origin.y -= kWindowStaggerOffset;
      
      // check if this new window position would overlap the dock or go off the screen. We test
      // this by ensuring that it is contained by the  visible screen rect (excluding dock). If
      // not, the window juts out somewhere and needs to be repositioned.
      if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
        // if a normal cascade fails, try shifting horizontally and reseting vertically
        testBrowserFrame.origin.y = NSMaxY(screenRect) - testBrowserFrame.size.height;
        if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
          // if shifting right also fails, try shifting vertically and reseting horizontally instead
          testBrowserFrame.origin.x = NSMinX(screenRect);
          testBrowserFrame.origin.y = previousOrigin.y - kWindowStaggerOffset;
          if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
            // if all else fails, give up and reset to the upper left corner
            testBrowserFrame.origin.x = NSMinX(screenRect);
            testBrowserFrame.origin.y = NSMaxY(screenRect) - testBrowserFrame.size.height;
          }
        }
      }
      // actually move the window
      [[self window] setFrameOrigin: testBrowserFrame.origin];
    }
    
    // if the search field is not on the toolbar, nil out the nextKeyView of the
    // url bar so that we know to break off the toolbar when tabbing. If it is,
    // and we're running on pre-panther, set the search bar as the tab view. We
    // don't want to do this on panther because it will do it for us.
    if (![mSearchBar window])
      [mURLBar setNextKeyView:nil];
    else {
      const float kPantherAppKit = 743.0;
      if (NSAppKitVersionNumber < kPantherAppKit)
        [mURLBar setNextKeyView:mSearchBar];
    }
    
    // needed when full keyboard access is enabled
    [mLock setRefusesFirstResponder:YES];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
	//if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
  //  return [[self window] frame].size;
	return proposedFrameSize;
}


#if 0
- (void)drawerWillOpen: (NSNotification*)aNotification
{
  [mBookmarkViewController ensureBookmarks];

  if ([[[mSidebarTabView selectedTabViewItem] identifier] isEqual:@"historySidebarCHIconTabViewItem"]) {
    [mHistoryDataSource loadLazily];
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
#endif

#pragma mark -

//
// -createToolbarPopupButton:
//
// Create a new instance of one of our special click-hold popup buttons that knows
// how to display a menu on click-hold. Associate it with the toolbar item |inItem|.
//
- (NSButton*)createToolbarPopupButton:(NSToolbarItem*)inItem
{
  NSRect frame = NSMakeRect(0.,0.,32.,32.);
  NSButton* button = [[[ToolbarButton alloc] initWithFrame:frame item:inItem] autorelease];
  if (button) {
    DraggableImageAndTextCell* newCell = [[[DraggableImageAndTextCell alloc] init] autorelease];
    [newCell setDraggable:YES];
    [newCell setClickHoldTimeout:0.75];
    [button setCell:newCell];

    [button setBezelStyle: NSRegularSquareBezelStyle];
    [button setButtonType: NSMomentaryChangeButton];
    [button setBordered: NO];
    [button setImagePosition: NSImageOnly];
  }
  return button;
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
  else if ( [[item itemIdentifier] isEqual:SearchToolbarItemIdentifier] ) {
    // restore the next key view of the url bar to the search bar, but only
    // if we're on jaguar. On panther, we really don't know that it should
    // be the search toolbar (it could be another toolbar button if full keyboard
    // access is enabled) but it will fix itself automatically.
    const float kPantherAppKit = 743.0;
    if (NSAppKitVersionNumber < kPantherAppKit)
      [mURLBar setNextKeyView:mSearchBar];
  }
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
  else if ( [[item itemIdentifier] isEqual:SearchToolbarItemIdentifier] ) {
    // search bar removed, set next key view of url bar to nil which tells
    // it to break out of the toolbar tab ring on a tab.
    [mURLBar setNextKeyView:nil];
  }

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
                                        SearchToolbarItemIdentifier,
                                        SidebarToolbarItemIdentifier,
                                        nil] );
}

// XXX use a dictionary to speed up the following?

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
  NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];
  if ( [itemIdent isEqual:BackToolbarItemIdentifier] && willBeInserted )
  {
    // create a new toolbar item that knows how to do validation
    toolbarItem = [[[ToolbarViewItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    
    NSButton* button = [self createToolbarPopupButton:toolbarItem];
    [toolbarItem setLabel:NSLocalizedString(@"Back", @"Back")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Back", @"Go Back")];
    [toolbarItem setToolTip:NSLocalizedString(@"BackToolTip", @"Go back one page")];

    NSSize size = NSMakeSize(32., 32.);
    NSImage* icon = [NSImage imageNamed:@"back"];
    [icon setScalesWhenResized:YES];
    [button setImage:icon];
    
    [toolbarItem setView:button];
    [toolbarItem setMinSize:size];
    [toolbarItem setMaxSize:size];
    
    [button setTarget:self];
    [button setAction:@selector(back:)];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(back:)];      // so validateToolbarItem: works correctly
    [[button cell] setClickHoldAction:@selector(backMenu:)];

    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(back:)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ([itemIdent isEqual:BackToolbarItemIdentifier]) {
    // not going onto the toolbar, don't need to go through the gynmastics above
    // and create a separate view
    [toolbarItem setLabel:NSLocalizedString(@"Back", @"Back")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Back", @"Go Back")];
    [toolbarItem setImage:[NSImage imageNamed:@"back"]];
  }
  else if ( [itemIdent isEqual:ForwardToolbarItemIdentifier] && willBeInserted )
  {
    // create a new toolbar item that knows how to do validation
    toolbarItem = [[[ToolbarViewItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    
    NSButton* button = [self createToolbarPopupButton:toolbarItem];
    [toolbarItem setLabel:NSLocalizedString(@"Forward", @"Forward")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Forward", @"Go Forward")];
    [toolbarItem setToolTip:NSLocalizedString(@"ForwardToolTip", @"Go forward one page")];

    NSSize size = NSMakeSize(32., 32.);
    NSImage* icon = [NSImage imageNamed:@"forward"];
    [icon setScalesWhenResized:YES];
    [button setImage:icon];

    [toolbarItem setView:button];
    [toolbarItem setMinSize:size];
    [toolbarItem setMaxSize:size];
    
    [button setTarget:self];
    [button setAction:@selector(forward:)];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(forward:)];      // so validateToolbarItem: works correctly
    [[button cell] setClickHoldAction:@selector(forwardMenu:)];

    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(forward:)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ( [itemIdent isEqual:ForwardToolbarItemIdentifier] ) {
    // not going onto the toolbar, don't need to go through the gynmastics above
    // and create a separate view
    [toolbarItem setLabel:NSLocalizedString(@"Forward", @"Forward")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Forward", @"Go Forward")];
    [toolbarItem setImage:[NSImage imageNamed:@"forward"]];
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
    [toolbarItem setLabel:NSLocalizedString(@"ToggleBookmarks", @"Manage Bookmarks label")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Manage Bookmarks", @"Manage Bookmarks palette")];
    [toolbarItem setToolTip:NSLocalizedString(@"BookmarkMgrToolTip", @"Show or hide all bookmarks")];
    [toolbarItem setImage:[NSImage imageNamed:@"manager"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(toggleSidebar:)];
  }
  else if ( [itemIdent isEqual:SearchToolbarItemIdentifier] )
  {
    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];

    [toolbarItem setLabel:NSLocalizedString(@"Search", @"Search")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Search", @"Search")];
    [toolbarItem setToolTip:NSLocalizedString(@"SearchToolTip", @"Search the Internet")];
    [toolbarItem setView:mSearchBar];
    [toolbarItem setMinSize:NSMakeSize(128, NSHeight([mSearchBar frame]))];
    [toolbarItem setMaxSize:NSMakeSize(150, NSHeight([mSearchBar frame]))];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(performSearch:)];

    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(beginSearchSheet)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
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
    [toolbarItem setMinSize:NSMakeSize(128, NSHeight([mLocationToolbarView frame]))];
    [toolbarItem setMaxSize:NSMakeSize(2560, NSHeight([mLocationToolbarView frame]))];

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
  if (action == @selector(back:)) {
    // if the bookmark manager is showing, we enable the back button so that
    // they can click back to return to the webpage they were viewing.
    BOOL enable = NO;
    if ( [mContentView isBookmarkManagerVisible] )
      enable = YES;
    else
      enable = [[mBrowserView getBrowserView] canGoBack];

    // we have to handle all the enabling/disabling ourselves because this
    // toolbar button is a view item. Note the return value is ignored.
    [(NSButton*)[theItem view] setEnabled:enable];
    return enable;
  }
  else if (action == @selector(forward:)) {
    // we have to handle all the enabling/disabling ourselves because this
    // toolbar button is a view item. Note the return value is ignored.
    BOOL enable = [[mBrowserView getBrowserView] canGoForward];
    [(NSButton*)[theItem view] setEnabled:enable];
    return enable;
  }
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
    return ![MainController isBlankURL:curURL];
  }
  else
    return YES;
}

#pragma mark -

- (void)loadingStarted
{
  [self startThrobber];
}

- (void)loadingDone
{
  [self stopThrobber];
  [mHistoryDataSource refresh];
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

- (void)performAppropriateSearchAction
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
        if ([[[itemsWeCanSee objectAtIndex:i] itemIdentifier] isEqual:SearchToolbarItemIdentifier])
        {
          [self focusSearchBar];
          return;
        }
      }
    }
  }

  [self beginSearchSheet];
}

- (void)focusSearchBar
{
  [[mBrowserView getBrowserView] setActive:NO];

  [mSearchBar selectText:self];
}

- (void)beginSearchSheet
{
  [NSApp beginSheet:  mSearchSheetWindow
     modalForWindow:  [self window]
      modalDelegate:  nil
     didEndSelector:  nil
        contextInfo:  nil];
}

- (IBAction)endSearchSheet:(id)sender
{
  [mSearchSheetWindow orderOut:self];
  [NSApp endSheet:mSearchSheetWindow returnCode:1];
  [self performSearch:mSearchSheetTextField];
}

- (IBAction)cancelSearchSheet:(id)sender
{
  [mSearchSheetWindow orderOut:self];
  [NSApp endSheet:mSearchSheetWindow returnCode:0];
}

-(IBAction)cancelAddBookmarkSheet:(id)sender
{
  [mAddBookmarkSheetWindow orderOut:self];
  [NSApp endSheet:mAddBookmarkSheetWindow returnCode:0];
  [mCachedBMVC endAddBookmark: 0];
}

-(IBAction)endAddBookmarkSheet:(id)sender
{
  [mAddBookmarkSheetWindow orderOut:self];
  [NSApp endSheet:mAddBookmarkSheetWindow returnCode:0];
  [mCachedBMVC endAddBookmark: 1];
}

- (void)cacheBookmarkVC:(BookmarkViewController *)aVC
{
  mCachedBMVC = aVC;
}

//
// - manageBookmarks:
//
// Toggle the bookmark manager in all cases. This is what users
// expect to happen. When the manager is displayed, retain the 
// last selected collection, regardless of what it is.
//
-(IBAction)manageBookmarks: (id)aSender
{
  [self toggleBookmarkManager: self];
}

//
// -manageHistory:
//
// History is a slightly different beast from bookmarks. Unlike 
// bookmarks, which acts as a toggle, history ensures the manager
// is visible and selects the history collection. If the manager
// is already visible, selects the history collection.
//
// An alternate solution would be to have it select history when
// it wasn't the selected container, and hide when history was
// the selected collection (toggling in that one case). This makes
// me feel dirty as the command does two different things depending
// on the (possibly undiscoverable to the user) context in which it is
// invoked. For that reason, I've chosen to not have history be a 
// toggle and see the fallout.
//
-(IBAction)manageHistory: (id)aSender
{
  if ( ![mContentView isBookmarkManagerVisible] )
    [self toggleBookmarkManager: self];

  [mBookmarkViewController selectContainer:kHistoryContainerIndex];
}

- (IBAction)goToLocationFromToolbarURLField:(id)sender
{
  // trim off any whitespace around url
  NSString *theURL = [[sender stringValue] stringByTrimmingWhitespace];
  
  // look for bookmarks keywords match
  NSArray *resolvedURLs = [[BookmarkManager sharedBookmarkManager] resolveBookmarksKeyword:theURL];

  NSString* resolvedURL = nil;
  if ([resolvedURLs count] == 1)
  {
    resolvedURL = [resolvedURLs lastObject];
    [self loadURL:resolvedURL referrer:nil activate:YES];
  }
  else
  {
  	[self openURLArray:resolvedURLs replaceExistingTabs:YES];
  }
    
  // global history needs to know the user typed this url so it can present it
  // in autocomplete. We use the URI fixup service to strip whitespace and remove
  // invalid protocols, etc. Don't save keyword-expanded urls.
  if (resolvedURL && [theURL isEqualToString:resolvedURL] && mGlobalHistory && mURIFixer && [theURL length] > 0)
  {
    nsAutoString url;
    [theURL assignTo_nsAString:url];
    NS_ConvertUCS2toUTF8 utf8URL(url);

    nsCOMPtr<nsIURI> fixedURI;
    mURIFixer->CreateFixupURI(utf8URL, 0, getter_AddRefs(fixedURI));
    if (fixedURI)
      mGlobalHistory->MarkPageAsTyped(fixedURI);
  }
}

- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView
{
  [[mBrowserView getBrowserView] saveDocument:focusedFrame filterView:aFilterView];
}

- (void)saveURL: (NSView*)aFilterView url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename
{
  [[mBrowserView getBrowserView] saveURL: aFilterView url: aURLSpec suggestedFilename: aFilename];
}

- (void)loadSourceOfURL:(NSString*)urlStr
{
  // first attempt to get the source that's already loaded
  PRBool loadInBackground;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  pref->GetBoolPref("browser.tabs.loadInBackground", &loadInBackground);

  nsCOMPtr<nsISupports> desc = [[mBrowserView getBrowserView] getPageDescriptor];
  if (desc) {
    // make sure we're not trying to load a subframe by checking |urlStr| against the url in
    // the desc (which is a history entry). We can only use the desc if it's the toplevel page.
    nsCOMPtr<nsIHistoryEntry> entry(do_QueryInterface(desc));
    if (entry) {
      nsCOMPtr<nsIURI> uri;
      entry->GetURI(getter_AddRefs(uri));
      nsCAutoString spec;
      uri->GetSpec(spec);
      if ([urlStr isEqualToString:[NSString stringWithUTF8String:spec.get()]]) {
        if ([self newTabsAllowed])
          [self openNewTabWithDescriptor:desc displayType:nsIWebPageDescriptor::DISPLAY_AS_SOURCE loadInBackground:loadInBackground];
        else
          [self openNewWindowWithDescriptor:desc displayType:nsIWebPageDescriptor::DISPLAY_AS_SOURCE loadInBackground:loadInBackground];
        return;
      }
    }
  }

  //otherwise reload it from the server
  NSString* viewSource = [@"view-source:" stringByAppendingString: urlStr];
  if ([self newTabsAllowed])
    [self openNewTabWithURL: viewSource referrer:nil loadInBackground: loadInBackground];
  else
    [self openNewWindowWithURL: viewSource referrer:nil loadInBackground: loadInBackground];
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
  // If we have a valid SearchTextField, perform a search using its contents
  if ([aSender isKindOfClass:[SearchTextField class]]) {
    // Get the search URL from our dictionary of sites and search urls
    NSMutableString *searchURL = [NSMutableString stringWithString:
      [[BrowserWindowController searchURLDictionary] objectForKey:
        [aSender titleOfSelectedPopUpItem]]];
    NSString *currentURL = [[self getBrowserWrapper] getCurrentURLSpec];
    NSString *searchString = [aSender stringValue];
    
    const char *aURLSpec = [currentURL lossyCString];
    NSString *aDomain = @"";
    nsIURI *aURI = nil;

    // If we have an about: type URL, remove " site:%d" from the search string
    // This is a fix to deal with Google's Search this Site feature
    // If other sites use %d to search the site, we'll have to have specific rules
    // for those sites.

    if ([currentURL hasPrefix:@"about:"]) {
      NSRange domainStringRange = [searchURL rangeOfString:@" site:%d"
                                                   options:NSBackwardsSearch];

      NSRange notFoundRange = NSMakeRange(NSNotFound, 0);
      if (NSEqualRanges(domainStringRange, notFoundRange) == NO)
        [searchURL deleteCharactersInRange:domainStringRange];
    }

    // If they didn't type anything in the search field, visit the domain of
    // the search site, i.e. www.google.com for the Google site
    if ([[aSender stringValue] isEqualToString:@""]) {
      aURLSpec = [searchURL lossyCString];

      if (NS_NewURI(&aURI, aURLSpec, nsnull, nsnull) == NS_OK) {
        nsCAutoString spec;
        aURI->GetHost(spec);

        aDomain = [NSString stringWithUTF8String:spec.get()];

        [self loadURL:aDomain referrer:nil activate:NO];
      }
    }
    else {
      aURLSpec = [[[self getBrowserWrapper] getCurrentURLSpec] lossyCString];

      // Get the domain so that we can replace %d in our searchURL
      if (NS_NewURI(&aURI, aURLSpec, nsnull, nsnull) == NS_OK) {
        nsCAutoString spec;
        aURI->GetHost(spec);

        aDomain = [NSString stringWithUTF8String:spec.get()];
      }

      // Escape the search string so the user can search for strings with
      // special characters ("&", "+", etc.) List from RFC2396.
      NSString *escapedSearchString = (NSString *) CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef)searchString, NULL, CFSTR(";/?:@&=+$,"), kCFStringEncodingUTF8);
      
      // replace the conversion specifiers (%d, %s) in the search string
      [self transformFormatString:searchURL domain:aDomain search:escapedSearchString];
      [escapedSearchString release];
      
      [self loadURL:searchURL referrer:nil activate:NO];
    }
  }
}

//
// - transformFormatString:domain:search
//
// Replaces all occurances of %d in |inFormat| with |inDomain| and all occurances of
// %s with |inSearch|. This is easy on jaguar and beyond, but not as easy on 10.1. Both
// implementations are presented here.
//
- (void) transformFormatString:(NSMutableString*)inFormat domain:(NSString*)inDomain search:(NSString*)inSearch
{
  if ([NSMutableString instancesRespondToSelector:
                          @selector(replaceOccurrencesOfString:withString:options:range:)]) {
    
    // If we're on Mac OS X >= 10.2...

    // Replace any occurence of %d with the current domain
    [inFormat replaceOccurrencesOfString:@"%d" withString:inDomain options:NSBackwardsSearch
                  range:NSMakeRange(0, [inFormat length])];

    // Replace any occurence of %s with the contents of the search text field
    [inFormat replaceOccurrencesOfString:@"%s" withString:inSearch options:NSBackwardsSearch
                  range:NSMakeRange(0, [inFormat length])];
  } 
  else {
    
    // If we're not on Mac OS X 10.2, do the string replacement manually

    NSRange notFoundRange = NSMakeRange(NSNotFound, 0);
    
    // Keep finding %d's and replacing them with the domain
    NSRange domainEscapeRange = [inFormat rangeOfString:@"%d" options:NSBackwardsSearch];

    while (NSEqualRanges(domainEscapeRange, notFoundRange) == NO) {
      [inFormat replaceCharactersInRange:domainEscapeRange withString:inDomain];

      // Get the next %d found. A domain can't contain a %d string,
      // so don't worry about that
      domainEscapeRange = [inFormat rangeOfString:@"%d" options:NSBackwardsSearch];
    }

    // Replace any occurence of %s with the contents of the search text field
    NSMutableArray *formatLocations = [[NSMutableArray alloc] init];
    NSScanner *formatScanner = [NSScanner scannerWithString:inFormat];
    NSString *tempString = nil;

    // Find the locations of all %s and store them in an NSMutableArray
    const int kStringConverterLen = 2;    // strlen("%s")
    [formatScanner scanUpToString:@"%s" intoString:nil];
    while ([formatScanner scanString:@"%s" intoString:&tempString]) {
      [formatLocations addObject:[NSNumber numberWithUnsignedInt:([formatScanner scanLocation] - kStringConverterLen)]];

      [formatScanner scanUpToString:@"%s" intoString:nil];
    }

    // Replace all %s in the string, starting at the end first to so we don't disrupt the
    // ranges we've computed
    NSRange formatRange;        
    for (int i = [formatLocations count] - 1; i >= 0; i--) {
      formatRange = NSMakeRange([[formatLocations objectAtIndex:i] unsignedIntValue], 2);
      [inFormat replaceCharactersInRange:formatRange withString:inSearch];
    }
    
    [formatLocations release];
  }
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

- (BOOL)findInPage:(BOOL)inBackwards
{
  return [[mBrowserView getBrowserView] findInPage:inBackwards];
}

- (NSString*)lastFindText
{
  return [[mBrowserView getBrowserView] lastFindText];
}

- (void)addBookmarkExtended:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  [mBookmarkViewController ensureBookmarks];
  [mBookmarkViewController addItem: self isFolder: aIsFolder URL:aURL title:aTitle];
}

// if bookmarks are visible, but say no if selection required and there isn't one
// if bookmarks aren't visible, or selection not required, then value for allowMultipleSelection is ignored
- (BOOL)bookmarksAreVisible:(BOOL)inRequireSelection allowMultipleSelection:(BOOL)allowMultipleSelection
{
  BOOL bookmarksShowing = [mContentView isBookmarkManagerVisible];
  // trying to make this logic as clear as possible
  if (bookmarksShowing && inRequireSelection) {
    bookmarksShowing = [mBookmarkViewController haveSelectedRow];
    if (bookmarksShowing && !allowMultipleSelection) {
      bookmarksShowing = [mBookmarkViewController numberOfSelectedRows] <= 1;
    }
  }
  return bookmarksShowing;
}

- (IBAction)bookmarkPage: (id)aSender
{
  [self addBookmarkExtended:NO URL:nil title:nil];
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
  [self addBookmarkExtended:NO URL:urlStr title:titleStr];
}

//
// -currentWebNavigation
//
// return a weak reference to the current web navigation object. Callers should
// not hold onto this for longer than the current call unless they addref it.
//
- (nsIWebNavigation*) currentWebNavigation
{
  BrowserWrapper* wrapper = [self getBrowserWrapper];
  if (!wrapper) return nsnull;
  CHBrowserView* view = [wrapper getBrowserView];
  if (!view) return nsnull;
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([view getWebBrowser]);
  if (!webBrowser) return nsnull;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webBrowser));
  return webNav.get();
}

//
// -sessionHistory
//
// Return a weak reference to the current session history object. Callers
// should not hold onto this for longer than the current call unless they addref.
//
- (nsISHistory*) sessionHistory
{
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return nil;
  nsCOMPtr<nsISHistory> sessionHistory;
  webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
  return sessionHistory.get();
}

- (void)historyItemAction:(id)inSender
{
  // get web navigation for current browser
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return;
  
  // browse to the history entry for the menuitem that was selected
  PRInt32 historyIndex = [inSender tag];
  webNav->GotoIndex(historyIndex);
}

//
// -populateSessionHistoryMenu:shist:from:to:
//
// Adds to the given popup menu the items of the session history from index |inFrom| to
// |inTo|. Sets the tag on the item to the index in the session history. When an item
// is selected, calls |-historyItemAction:|. Correctly handles iterating in the 
// correct direction based on relative indices of from/to.
//
- (void)populateSessionHistoryMenu:(NSMenu*)inPopup shist:(nsISHistory*)inHistory from:(unsigned long)inFrom to:(unsigned long)inTo
{
  // max number of characters in a menu title before cropping it
  const unsigned int kMaxTitleLength = 80;

  // go forwards if |inFrom| < |inTo| and backwards if |inFrom| > |inTo|
  int direction = -1;
  if (inFrom <= inTo)
    direction = 1;

  // create a menu item for each item in the session history. Instead of simply going
  // from |inFrom| to |inTo|, we use |count| to take the directionality out of the loop
  // control so we can go fwd or backwards with the same loop control.
  const int numIterations = abs(inFrom - inTo) + 1;    
  for (PRInt32 i = inFrom, count = 0; count < numIterations; i += direction, ++count) {
    nsCOMPtr<nsIHistoryEntry> entry;
    inHistory->GetEntryAtIndex(i, PR_FALSE, getter_AddRefs(entry));
    if (entry) {
      nsXPIDLString textStr;
      entry->GetTitle(getter_Copies(textStr));
      NSString* title = [[NSString stringWith_nsAString: textStr] stringByTruncatingTo:kMaxTitleLength at:kTruncateAtMiddle];    
      NSMenuItem *newItem = [inPopup addItemWithTitle:title action:@selector(historyItemAction:) keyEquivalent:@""];
      [newItem setTarget:self];
      [newItem setTag:i];
    }
  }
}

//
// -forwardMenu:
//
// Create a menu off the fwd button (the sender) that contains the session history
// from the current position forward to the most recent in the session history.
//
- (IBAction)forwardMenu:(id)inSender
{
  NSMenu* popupMenu = [[[NSMenu alloc] init] autorelease];
  [popupMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];  // dummy first item

  // figure out what indices of the history to build in the menu. Item 0
  // in the shared history is the least recent (beginning of history) page.
  nsISHistory* sessionHistory = [self sessionHistory];
  PRInt32 currentIndex;
  sessionHistory->GetIndex(&currentIndex);
  PRInt32 count;
  sessionHistory->GetCount(&count);

  // builds forwards from the item after the current to the end (|count|)
  [self populateSessionHistoryMenu:popupMenu shist:sessionHistory from:currentIndex+1 to:count-1];

  // use a temporary NSPopUpButtonCell to display the menu.
  NSPopUpButtonCell *popupCell = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES] autorelease];
  [popupCell setMenu: popupMenu];
  [popupCell trackMouse:[NSApp currentEvent] inRect:[inSender bounds] ofView:inSender untilMouseUp:YES];
}

//
// -backMenu:
//
// Create a menu off the back button (the sender) that contains the session history
// from the current position backward to the first item in the session history.
//
- (IBAction)backMenu:(id)inSender
{
  // do nothing on click-hold if the bm manager is visible
  if ( [mContentView isBookmarkManagerVisible] )
    return;

  NSMenu* popupMenu = [[[NSMenu alloc] init] autorelease];
  [popupMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];  // dummy first item

  // figure out what indices of the history to build in the menu. Item 0
  // in the shared history is the least recent (beginning of history) page.
  nsISHistory* sessionHistory = [self sessionHistory];
  PRInt32 currentIndex;
  sessionHistory->GetIndex(&currentIndex);

  // builds backwards from the item before the current item to 0, the first item in the list
  [self populateSessionHistoryMenu:popupMenu shist:sessionHistory from:currentIndex-1 to:0];

  // use a temporary NSPopUpButtonCell to display the menu.
  NSPopUpButtonCell *popupCell = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES] autorelease];
  [popupCell setMenu: popupMenu];
  [popupCell trackMouse:[NSApp currentEvent] inRect:[inSender bounds] ofView:inSender untilMouseUp:YES];
}

- (IBAction)back:(id)aSender
{
  // if the bookmark manager is visible, hitting the back button will restore
  // the browser views, not actually go back.
  if ( [mContentView isBookmarkManagerVisible] )
    [self toggleBookmarkManager:self];
  else
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
#if USE_DRAWER_FOR_BOOKMARKS
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
#else
  [self toggleBookmarkManager:self];
#endif
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

//
// isPageTextFieldFocused
//
// Determine if a text field in the content area has focus. Returns YES if the
// focus is in a <input type="text"> or <textarea>
//
- (BOOL)isPageTextFieldFocused
{
  #define ENSURE_TRUE(x) if (!x) return NO;
  BOOL isFocused = NO;
  
  nsCOMPtr<nsIWebBrowser> webBrizzle = dont_AddRef([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  ENSURE_TRUE(webBrizzle);
  nsCOMPtr<nsIDOMWindow> domWindow;
  webBrizzle->GetContentDOMWindow(getter_AddRefs(domWindow));
  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(domWindow);
  ENSURE_TRUE(privateWindow);
  nsIFocusController *controller = privateWindow->GetRootFocusController();
  ENSURE_TRUE(controller);
  nsCOMPtr<nsIDOMElement> focusedItem;
  controller->GetFocusedElement(getter_AddRefs(focusedItem));
  
  // we got it, now check if it's what we care about
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(focusedItem);
  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(focusedItem);
  if (input) {
    nsAutoString type;
    input->GetType(type);
    if (type == NS_LITERAL_STRING("text"))
      isFocused = YES;
  }
  else if (textArea)
    isFocused = YES;
  
  return isFocused;
}

// map delete key to Back
- (void)deleteBackward:(id)sender
{
  // there are times when backspaces can seep through from IME gone wrong. As a 
  // workaround until we can get them all fixed, ignore backspace when the
  // focused widget is a text field (<input type="text"> or <textarea>
  if ([self isPageTextFieldFocused])
    return;
  
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
      locationString = @""; // return;

    [mURLBar setURI:locationString];
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
    
    [self ensureBrowserVisible:self];
    
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

      focusURLBar = locationBarVisible && [MainController isBlankURL:urlToLoad];      

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
  BrowserWindowController* browser = [self openNewWindow:aLoadInBG];
  [browser loadURL: aURLSpec referrer:aReferrer activate:!aLoadInBG];
}

//
// -openNewWindow:
//
// open a new window, but doesn't load anything into it. Must be matched
// with a call to do that.
//
- (BrowserWindowController*)openNewWindow:(BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  if (aLoadInBG)
  {
    BrowserWindow* browserWin = [browser window];
    [browserWin setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [browserWin orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
    [browserWin setSuppressMakeKeyFront:NO];
  }
  else
    [browser showWindow:self];

  return browser;
}

-(void)openNewTabWithURL: (NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground: (BOOL)aLoadInBG
{
  BrowserTabViewItem* newTab = [self openNewTab:aLoadInBG];
  [[newTab view] loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone activate:!aLoadInBG];
}

//
// -openNewTab:
//
// open a new tab, but doesn't load anything into it. Must be matched
// with a call to do that.
//
- (BrowserTabViewItem*)openNewTab:(BOOL)aLoadInBG;
{
  BrowserTabViewItem* newTab  = [self createNewTabItem];

  [self ensureBrowserVisible:self];
  
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

  // unless we're told to load this tab in the bg, select the tab
  // before we load so that it's the primary and will push the url into
  // the url bar immediately rather than waiting for the server.
  if (!aLoadInBG)
    [mTabBrowser selectTabViewItem: newTab];

  return newTab;
}

-(void)openNewWindowWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
{
  BrowserWindowController* browser = [self openNewWindow:aLoadInBG];
  [[[browser getBrowserWrapper] getBrowserView] setPageDescriptor:aDesc displayType:aDisplayType];
}

-(void)openNewTabWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
{
  BrowserTabViewItem* newTab = [self openNewTab:aLoadInBG];
  [[[newTab view] getBrowserView] setPageDescriptor:aDesc displayType:aDisplayType];
}

- (void)openURLArray:(NSArray*)urlArray replaceExistingTabs:(BOOL)replaceExisting
{
  // ensure the content area is visible. We can't rely on normal url loading
  // to do this because for the new tabs we create below, they won't be connected
  // to their controller until much later, so the call to ensure visibility fails.
  [self ensureBrowserVisible:self];

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

#if USE_DRAWER_FOR_BOOKMARKS

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

#endif

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
  [mBookmarkViewController ensureBookmarks];
  [mBookmarkViewController showBookmarkInfo:sender];
}

- (BOOL)canGetInfo
{
  return [self bookmarksAreVisible:YES allowMultipleSelection:NO];
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
  else if (!mContextMenuFlags || (mContextMenuFlags & nsIContextMenuListener::CONTEXT_DOCUMENT) != 0)
  {
    // if there aren't any flags or we're in the background of a page,
    // show the document menu. This prevents us from failing to find a case
    // and not showing the context menu.
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
  NSView* accessoryView = [[NSApp delegate] getSavePanelView];
  [self saveDocument:NO filterView:accessoryView];
}

- (IBAction)saveFrameAs:(id)aSender
{
  NSView* accessoryView = [[NSApp delegate] getSavePanelView];
  [self saveDocument:YES filterView:accessoryView];
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

  [self saveURL:nil url:hrefStr suggestedFilename:[NSString stringWith_nsAString:text]];
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

      [self saveURL:nil url:hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
  }
}

- (IBAction)copyImage:(id)sender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(webBrowser));
  if (clipboard)
    clipboard->CopyImageContents();
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

- (BookmarkToolbar*) bookmarkToolbar
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
// - showPopupBlocked:
//
// Show/hide the image of the blocked-popup indicator
//
- (void)showPopupBlocked:(BOOL)inBlocked
{
  if ( inBlocked && ![mPopupBlocked window] ) {       // told to show, currently hidden
    [mPopupBlockSuperview addSubview:mPopupBlocked];
  }
  else if ( !inBlocked && [mPopupBlocked window] ) {  // told to hide, currently visible                               
    [mPopupBlocked removeFromSuperview];
  }
}

//
// buildPopupBlockerMenu:
//
// Called by the notification center right before the menu will be displayed. This
// allows us the opportunity to populate its contents from the list of sites
// in the block list.
//
- (void)buildPopupBlockerMenu:(NSNotification*)notifier
{
  const long kSeparatorTag = -1;
  NSPopUpButton* popup = [notifier object];
  
  // clear out existing menu. loop until we hit our special tag
  int numItemsToDelete = [popup indexOfItemWithTag:kSeparatorTag];
  for ( int i = 0; i < numItemsToDelete; ++i ) 
    [popup removeItemAtIndex:0];
    
  // the first item will get swallowed by the popup
  [popup insertItemWithTitle:@"" atIndex:0];
  
  // fill in new menu
  nsCOMPtr<nsISupportsArray> blockedSites;
  [[self getBrowserWrapper] getBlockedSites:getter_AddRefs(blockedSites)];
  PRUint32 siteCount = 0;
  blockedSites->Count(&siteCount);
  for ( PRUint32 i = 0, insertAt = 1; i < siteCount; ++i ) {
    nsCOMPtr<nsISupports> genericURI = dont_AddRef(blockedSites->ElementAt(i));
    nsCOMPtr<nsIURI> uri = do_QueryInterface(genericURI);
    if ( uri ) {
      // extract the host
      nsCAutoString host;
      uri->GetHost(host);
      NSString* hostString = [NSString stringWithCString:host.get()];      
      NSString* itemTitle = [NSString stringWithFormat:NSLocalizedString(@"Unblock %@", @"Unblock %@"), hostString];

      // ensure that duplicate hosts aren't inserted
      if ([popup indexOfItemWithTitle:itemTitle] == -1) {
        // create a new menu item and set its tag to the position in the menu so
        // the action can know which site we want to unblock. Insert it at |i+1|
        // because we had to pad with one item above, but set the tag to |i| because
        // that's the index in the array.
        [popup insertItemWithTitle:itemTitle atIndex:insertAt];
        NSMenuItem* currItem = [popup itemAtIndex:insertAt];
        [currItem setAction:@selector(unblockSite:)];
        [currItem setTarget:self];
        [currItem setTag:i];
        ++insertAt;              // only increment insert pos if we inserted something
      }
    }
  }
}

//
// unblockSite:
//
// Called in response to an item in the unblock popup menu being selected to
// add a particular site to the popup whitelist. We assume that the tag of
// the sender is the index into the blocked sites array stores in the browser 
// wrapper to get the nsURI.
//
- (IBAction)unblockSite:(id)sender
{
  nsCOMPtr<nsISupportsArray> blockedSites;
  [[self getBrowserWrapper] getBlockedSites:getter_AddRefs(blockedSites)];

  // get the tag from the sender and use that as the index into the list
  long tag = [sender tag];
  if ( tag >= 0 ) {
    nsCOMPtr<nsISupports> genUri = dont_AddRef(blockedSites->ElementAt(tag));
    nsCOMPtr<nsIURI> uri = do_QueryInterface(genUri);

    nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );
    pm->Add(uri, "popup", nsIPermissionManager::ALLOW_ACTION);
  }
}

//
// - unblockAllSites:
//
// Called in response to the menu item from the unblock popup. Loop over all
// the items in the blocked sites array in the browser wrapper and add them
// to the whitelist.
//
- (IBAction)unblockAllSites:(id)sender
{
  nsCOMPtr<nsISupportsArray> blockedSites;
  [[self getBrowserWrapper] getBlockedSites:getter_AddRefs(blockedSites)];
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );

  PRUint32 count = 0;
  blockedSites->Count(&count);
  for ( PRUint32 i = 0; i < count; ++i ) {
    nsCOMPtr<nsISupports> genUri = dont_AddRef(blockedSites->ElementAt(i));
    nsCOMPtr<nsIURI> uri = do_QueryInterface(genUri);
    pm->Add(uri, "popup", nsIPermissionManager::ALLOW_ACTION);   
  }
}

//
// -configurePopupBlocking
//
// Show the web features pref panel where the user can do things to configure
// popup blocking
//
- (IBAction)configurePopupBlocking:(id)sender
{
  [[NSApp delegate] displayPreferencesWindow:self];
  [[[NSApp delegate] preferencesController] selectPreferencePaneByIdentifier:@"org.mozilla.chimera.preference.webfeatures"];
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

// return the window's saved title
- (NSString *)savedTitle
{
  return mSavedTitle;
}

// save the window title before showing
// bookmark manager or History manager 
- (void)setSavedTitle:(NSString *)aTitle
{
  [mSavedTitle autorelease];
  mSavedTitle = [aTitle retain];
}

+ (NSDictionary *)searchURLDictionary
{
  static NSDictionary *searchURLDictionary = nil;

  if (searchURLDictionary == nil)
    searchURLDictionary = [[NSDictionary alloc] initWithContentsOfFile:
      [[NSBundle mainBundle] pathForResource:@"SearchURLList" ofType:@"plist"]];
  
  return searchURLDictionary;
}



- (void) focusChangedFrom:(NSResponder*) oldResponder to:(NSResponder*) newResponder
{
  BOOL oldResponderIsGecko = [self isResponderGeckoView:oldResponder];
  BOOL newResponderIsGecko = [self isResponderGeckoView:newResponder];

  if (oldResponderIsGecko != newResponderIsGecko && [[self window] isKeyWindow])
    [[mBrowserView getBrowserView] setActive:newResponderIsGecko];
}


- (PageProxyIcon *)proxyIconView
{
  return mProxyIcon;
}

- (BookmarkViewController *)bookmarkViewController
{
  return mBookmarkViewController;
}

- (id)windowWillReturnFieldEditor:(NSWindow *)aWindow toObject:(id)anObject
{
  if ([anObject isEqual:mURLBar]) {
    if (!mURLFieldEditor) {
      mURLFieldEditor = [[AutoCompleteTextFieldEditor alloc] initWithFrame:[anObject bounds]
                                                               defaultFont:[mURLBar font]];
      [mURLFieldEditor setFieldEditor:YES];
      [mURLFieldEditor setAllowsUndo:YES];
    }
    return mURLFieldEditor;
  }
  return nil;
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)sender
{
  return [[BookmarkManager sharedBookmarkManager] undoManager];
}

- (IBAction)reloadWithNewCharset:(NSString*)charset
{
  [mBrowserView reloadWithNewCharset:charset];
}

- (NSString*)currentCharset
{
  return [mBrowserView currentCharset];
}


//
// -toggleBookmarkManager
//
// switch between a gecko content view and the in-window bookmark manager.
// This changes the current focus and forces it into the content area.
//
- (void)toggleBookmarkManager:(id)sender
{
  // lazily init the setup of the view's controller
  if (!mBookmarkViewControllerInitialized) {
    [mBookmarkViewController completeSetup];
    mBookmarkViewControllerInitialized = YES;
  }
  
  // deactivate any gecko view that might think it has focus
  if ([self isResponderGeckoView:[[self window] firstResponder]]) {
    CHBrowserView* browserView = [mBrowserView getBrowserView];
    if (browserView)
      [browserView setActive:NO];
  }
  
  // swap out between content and bookmarks.
  [mContentView toggleBookmarkManager:sender];
    
  // if we're now showing the bm manager, force it to have focus,
  // otherwise give focus back to gecko.
  if ( [mContentView isBookmarkManagerVisible] ) {
    // cancel all pending loads. safari does this, i think we should too
    [self stopAllPendingLoads];
    
    // save window title
    [self setSavedTitle:[[self window] title]];
    [[self window] setTitle:NSLocalizedString(@"Bookmark Manager",@"Bookmark Manager")];
    [mBookmarkViewController selectLastContainer];

    // set focus to appropriate area of bm manager
    [mBookmarkViewController focus];
  }
  else {
    [[self window] setTitle:[self savedTitle]];
    CHBrowserView* browserView = [mBrowserView getBrowserView];
    if (browserView)
      [browserView setActive:YES];
  }

  // we have to manually update the bookmarks menu items, because we
  // turn autoenabling off for that menu
  [[NSApp delegate] adjustBookmarksMenuItemsEnabling:[[self window] isMainWindow]];
}

//
// - ensureBrowserVisible:
//
// Make sure that the browser is showing, not the bookmarks manager
//
- (void)ensureBrowserVisible:(id)sender
{
  if ( [mContentView isBookmarkManagerVisible] )
    [self toggleBookmarkManager:self];
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
