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

#import "NSString+Utils.h"
#import "NSView+Utils.h"

#import "PreferenceManager.h"
#import "BrowserWrapper.h"
#import "BrowserWindowController.h"
#import "BookmarksClient.h"
#import "SiteIconProvider.h"
#import "BrowserTabView.h"
#import "BrowserTabViewItem.h"
#import "ToolTip.h"
#import "PageProxyIcon.h"
#import "KeychainService.h"
#import "AutoCompleteTextField.h"

#include "CHBrowserService.h"
#include "ContentClickListener.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserSetup.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIChromeEventHandler.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIWebProgressListener.h"
#include "nsIBrowserDOMWindow.h"


static NSString* const kOfflineNotificationName = @"offlineModeChanged";

@interface BrowserWrapper(Private)

- (void)ensureContentClickListeners;

- (void)setPendingActive:(BOOL)active;
- (void)registerNotificationListener;

- (void)setSiteIconImage:(NSImage*)inSiteIcon;
- (void)setSiteIconURI:(NSString*)inSiteIconURI;

- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI loadError:(BOOL)inLoadError;

- (void)setTabTitle:(NSString*)tabTitle windowTitle:(NSString*)windowTitle;
- (NSString*)displayTitleForPageURL:(NSString*)inURL title:(NSString*)inTitle;

- (void)updateOfflineStatus;

- (void)checkForCustomViewOnLoad:(NSString*)inURL;

- (void)removeBlockedPopupViewAndDisplay;

@end

#pragma mark -

@implementation BrowserWrapper

- (id)initWithTab:(NSTabViewItem*)aTab inWindow:(NSWindow*)window
{
  if (([self initWithFrame:NSZeroRect inWindow:window]))
  {
    mTabItem = aTab;
  }
  return self;
}

//
// initWithFrame:  (designated initializer)
//
// Create a Gecko browser view and hook everything up to the UI
//
- (id)initWithFrame:(NSRect)frameRect inWindow:(NSWindow*)window
{
  if ((self = [super initWithFrame: frameRect]))
  {
    mWindow = window;

    // We retain the browser view so that we can rip it out for custom view support    
    mBrowserView = [[CHBrowserView alloc] initWithFrame:[self bounds] andWindow:window];
    [self addSubview:mBrowserView];

    [mBrowserView setContainer:self];
    [mBrowserView addListener:self];

    [[KeychainService instance] addListenerToView:mBrowserView];

    mIsBusy = NO;
    mListenersAttached = NO;
    mSecureState = nsIWebProgressListener::STATE_IS_INSECURE;
    mProgress = 0.0;
    mBlockedSites = nsnull;

    BOOL gotPref;
    BOOL pluginsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:"camino.enable_plugins" withSuccess:&gotPref];
    if (gotPref && !pluginsEnabled)
      [mBrowserView setProperty:nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS toValue:PR_FALSE];
    
    mToolTip = [[ToolTip alloc] init];

    //[self setSiteIconImage:[NSImage imageNamed:@"globe_ico"]];
    //[self setSiteIconURI: [NSString string]];
    
    mDefaultStatusString = [[NSString alloc] init];
    mLoadingStatusString = [[NSString alloc] init];

    mTitle = [[NSString alloc] init];
    
    [self registerNotificationListener];
  }
  return self;
}

-(void)dealloc
{
#if DEBUG
  NSLog(@"The browser wrapper died.");
#endif

  [[NSNotificationCenter defaultCenter] removeObserver: self];
  
  [mSiteIconImage release];
  [mSiteIconURI release];
  [mDefaultStatusString release];
  [mLoadingStatusString release];

  [mToolTip release];
  [mTitle release];
  [mTabTitle release];
  
  NS_IF_RELEASE(mBlockedSites);
  
  [mBrowserView release];
  [mContentViewProviders release];

  // |mBlockedPopupView| has a retain count of 1 when it comes out of the nib,
  // we have to release it manually.
  [mBlockedPopupView release];
  
  [super dealloc];
}

- (BOOL)isFlipped
{
  return YES;
}

-(void)windowClosed
{
  // Break the cycle, but don't clear ourselves as the container 
  // before we call |destroyWebBrowser| or onUnload handlers won't be
  // able to create new windows. The container will get cleared
  // when the CHBrowserListener goes away as a result of the
  // |destroyWebBrowser| call. (bug 174416)
  [mBrowserView removeListener: self];
  [mBrowserView destroyWebBrowser];

  // We don't want site icon notifications when the window has gone away
  [[NSNotificationCenter defaultCenter] removeObserver:self name:SiteIconLoadNotificationName object:nil];
  // We're basically a zombie now. Clear fields which are in an undefined state.
  mDelegate = nil;
  mWindow = nil;
}

- (void)setUICreationDelegate:(id<BrowserUICreationDelegate>)delegate
{
  mCreateDelegate = delegate;
}

- (void)setDelegate:(id<BrowserUIDelegate>)delegate
{
  mDelegate = delegate;
}

- (id<BrowserUIDelegate>)delegate
{
  return mDelegate;
}

-(void)setTab: (NSTabViewItem*)tab
{
  mTabItem = tab;
}

-(NSTabViewItem*)tab
{
  return mTabItem;
}

-(NSString*)getCurrentURI
{
  return [mBrowserView getCurrentURI];
}

- (void)setFrame:(NSRect)frameRect
{
  [self setFrame:frameRect resizingBrowserViewIfHidden:NO];
}

- (void)setFrame:(NSRect)frameRect resizingBrowserViewIfHidden:(BOOL)inResizeBrowser
{
  [super setFrame:frameRect];

  // Only resize our browser view if we are visible, unless the caller requests it.
  // If we're hidden, the frame will get reset when we get placed back into the
  // view hierarchy anyway. This enhancement keeps resizing in a window with
  // many tabs from being slow.
  // However, this requires us to do the resize on loadURI: below to make
  // sure that we maintain the scroll position in background tabs correctly.
  if ([self window] || inResizeBrowser)
  {
    NSRect bounds = [self bounds];
    if (mBlockedPopupView) {
      // resize the browser view and move it down by the height of the block popup
      // view. Recall we're flipped, so the origin is the top left. Also make sure
      // to apply changes for our new frame.
      NSRect popupBlockFrame = [mBlockedPopupView frame];
      NSRect browserFrame = [mBrowserView frame];
      browserFrame.origin.y = popupBlockFrame.size.height;
      browserFrame.size.width = bounds.size.width;
      browserFrame.size.height = bounds.size.height-popupBlockFrame.size.height;
      
      popupBlockFrame.origin = NSZeroPoint;
      popupBlockFrame.size.width = bounds.size.width;
      
      [mBrowserView setFrame:browserFrame];
      [mBlockedPopupView setFrame:popupBlockFrame];
    }
    else
      [mBrowserView setFrame:bounds];
  }
}

- (void)setBrowserActive:(BOOL)inActive
{
  [mBrowserView setActive:inActive];
}

-(BOOL)isBusy
{
  return mIsBusy;
}

- (NSString*)windowTitle
{
  return mTitle;
}

- (NSString*)pageTitle
{
  return [mBrowserView pageTitle];
}

- (NSImage*)siteIcon
{
  return mSiteIconImage;
}

- (NSString*)location
{
  return [mBrowserView getCurrentURI];
}

- (NSString*)statusString
{
  // XXX is this right?
  return mDefaultStatusString ? mDefaultStatusString : mLoadingStatusString;
}

- (float)loadingProgress
{
  return mProgress;
}

- (BOOL)popupsBlocked
{
  if (!mBlockedSites) return NO;
  
  PRUint32 numBlocked = 0;
  mBlockedSites->Count(&numBlocked);
  
  return (numBlocked > 0);
}

- (unsigned long)securityState
{
  return mSecureState;
}


- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags activate:(BOOL)activate allowPopups:(BOOL)inAllowPopups
{
  // blast it into the urlbar immediately so that we know what we're 
  // trying to load, even if it doesn't work
  [mDelegate updateLocationFields:urlSpec ignoreTyping:YES];
  
  // if we're not the primary tab, make sure that the browser view is 
  // the correct size when loading a url so that if the url is a relative
  // anchor, which will cause a scroll to the anchor on load, the scroll
  // position isn't messed up when we finally display the tab.
  if (mDelegate == nil)
  {
    NSRect tabContentRect = [[[mWindow delegate] getTabBrowser] contentRect];
    [self setFrame:tabContentRect resizingBrowserViewIfHidden:YES];
  }
  
  [self setPendingActive:activate];
  [mBrowserView loadURI:urlSpec referrer:referrer flags:flags allowPopups:inAllowPopups];
}

- (void)ensureContentClickListeners
{
  if (!mListenersAttached)
  {
    mListenersAttached = YES;
    
    // We need to hook up our click and context menu listeners.
    ContentClickListener* clickListener = new ContentClickListener([mWindow delegate]);
    if (!clickListener)
      return;
    
    nsCOMPtr<nsIDOMWindow> contentWindow = [[self getBrowserView] getContentWindow];
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(contentWindow));
    nsIChromeEventHandler *chromeHandler = piWindow->GetChromeEventHandler();
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(chromeHandler));
    if ( rec )
      rec->AddEventListenerByIID(clickListener, NS_GET_IID(nsIDOMMouseListener));
  }
}

- (void)didBecomeActiveBrowser
{
  [self ensureContentClickListeners];
  [self updateOfflineStatus];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(offlineModeChanged:)
                                               name:kOfflineNotificationName
                                             object:nil];

}

-(void)willResignActiveBrowser
{
  [mToolTip closeToolTip];

  [[NSNotificationCenter defaultCenter] removeObserver:self name:kOfflineNotificationName object:nil];
  [mBrowserView setActive:NO];
}


#pragma mark -

// custom view support

- (void)registerContentViewProvider:(id<ContentViewProvider>)inProvider forURL:(NSString*)inURL
{
  if (!mContentViewProviders)
    mContentViewProviders = [[NSMutableDictionary alloc] init];
    
  NSString* lowercaseURL = [inURL lowercaseString];
  [mContentViewProviders setObject:inProvider forKey:lowercaseURL];
}

- (void)unregisterContentViewProviderForURL:(NSString*)inURL
{
  [mContentViewProviders removeObjectForKey:[inURL lowercaseString]];
}

- (id)contentViewProviderForURL:(NSString*)inURL
{
  return [mContentViewProviders objectForKey:[inURL lowercaseString]];
}

- (void)checkForCustomViewOnLoad:(NSString*)inURL
{
  id<ContentViewProvider> provider = [mContentViewProviders objectForKey:[inURL lowercaseString]];
  NSView* providedView = [provider provideContentViewForURL:inURL];   // ok with nil provider

  NSView* newContentView = providedView ? providedView : mBrowserView;

  if ([self firstSubview] != newContentView)
  {
    [self swapFirstSubview:newContentView];
    [mDelegate contentViewChangedTo:newContentView forURL:inURL];
    
    // tell the provider that we swapped in its view
    if (providedView)
      [provider contentView:providedView usedForURL:inURL];
  }
}

#pragma mark -

- (void)onLoadingStarted 
{
  [mDefaultStatusString autorelease];
  mDefaultStatusString = nil;

  mProgress = 0.0;
  mIsBusy = YES;

  [mDelegate loadingStarted];
  [mDelegate setLoadingActive:YES];
  [mDelegate setLoadingProgress:mProgress];
  
  [mLoadingStatusString autorelease];
  mLoadingStatusString = [NSLocalizedString(@"TabLoading", @"") retain];
  [mDelegate updateStatus:mLoadingStatusString];
  
  [(BrowserTabViewItem*)mTabItem startLoadAnimation];

  [mTabTitle autorelease];
  mTabTitle = [mLoadingStatusString retain];
  [mTabItem setLabel:mTabTitle];
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  [mDelegate loadingDone:mActivateOnLoad];
  mActivateOnLoad = NO;
  
  [mDelegate setLoadingActive:NO];

  [mLoadingStatusString autorelease];
  mLoadingStatusString = [@"" retain];
  
  [mDelegate updateStatus:mDefaultStatusString ? mDefaultStatusString : mLoadingStatusString];
  
  [(BrowserTabViewItem*)mTabItem stopLoadAnimation];

  NSString *urlString = nil;
  NSString *titleString = nil;
  [self getTitle:&titleString andHref:&urlString];
  
  // If we never got a page title, then the tab title will be stuck at "Loading..."
  // so be sure to set the title here
  NSString* tabTitle = [self displayTitleForPageURL:urlString title:titleString];
  [mTabItem setLabel:tabTitle];
  
  mProgress = 1.0;
  mIsBusy = NO;

  // tell the bookmarks when a url loaded.
  // note that this currently fires even when you go Back of Forward to the page,
  // so it's not a great way to count bookmark visits.
  if (urlString && ![urlString isEqualToString:@"about:blank"])
  {
    NSDictionary*   userInfo = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:succeeded] forKey:URLLoadSuccessKey];
    NSNotification* note     = [NSNotification notificationWithName:URLLoadNotification object:urlString userInfo:userInfo];
    [[NSNotificationQueue defaultQueue] enqueueNotification:note postingStyle:NSPostWhenIdle];
  }

  // We've defered display of the blocked popup view until the page has finished loading in 
  // order to avoid jumping around during the page load. Even if we're hidden, we ensure that
  // the new view is in the view hierarchy and it will be resized when the
  // current tab is eventually displayed.
  if ([self popupsBlocked] && !mBlockedPopupView) {
    [NSBundle loadNibNamed:@"PopupBlockView" owner:self];
    [self addSubview:mBlockedPopupView];
    [self setFrame:[self frame] resizingBrowserViewIfHidden:YES];
    [self display];
  }
}

- (void)onProgressChange64:(long long)currentBytes outOf:(long long)maxBytes 
{
  if (maxBytes > 0)
  {
    mProgress = ((double)currentBytes / (double)maxBytes) * 100.0;
    [mDelegate setLoadingProgress:mProgress];
  }
}

- (void)onProgressChange:(long)currentBytes outOf:(long)maxBytes 
{
  if (maxBytes > 0)
  {
    mProgress = ((double)currentBytes / (double)maxBytes) * 100.0;
    [mDelegate setLoadingProgress:mProgress];
  }
}

- (void)onLocationChange:(NSString*)urlSpec isNewPage:(BOOL)newPage requestSucceeded:(BOOL)requestOK
{
  if (newPage)
  {
    // defer hiding of blocked popup view until we've loaded the new page
    [self removeBlockedPopupViewAndDisplay];
    NS_IF_RELEASE(mBlockedSites);
    [mDelegate showPopupBlocked:NO];
  
    NSString* faviconURI = [SiteIconProvider defaultFaviconLocationStringFromURI:urlSpec];
    if (requestOK && [faviconURI length] > 0)
    {
      SiteIconProvider* faviconProvider = [SiteIconProvider sharedFavoriteIconProvider];

      // if the favicon uri has changed, do the favicon load
      if (![faviconURI isEqualToString:mSiteIconURI])
      {
        // first get a cached image for this site, if we have one. we'll go ahead
        // and request the load anyway, in case the site updated their icon.
        NSImage*  cachedImage = [faviconProvider favoriteIconForPage:urlSpec];
        NSString* cachedImageURI = nil;

        if (cachedImage)
          cachedImageURI = faviconURI;
        
        // immediately update the site icon (to the cached one, or the default)
        [self updateSiteIconImage:cachedImage withURI:cachedImageURI loadError:NO];

        if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL])
        {
          // note that this is the only time we hit the network for site icons.
          // note also that we may get a site icon from a link element later,
          // which will replace any we get from the default location.
          // when this completes, our imageLoadedNotification: will get called.
          [faviconProvider fetchFavoriteIconForPage:urlSpec
                                   withIconLocation:nil
                                       allowNetwork:YES
                                    notifyingClient:self];
        }
      }
    }
    else
    {
      if ([urlSpec hasPrefix:@"about:"])
        faviconURI = urlSpec;
      else
      	faviconURI = @"";

      [self updateSiteIconImage:nil withURI:faviconURI loadError:!requestOK];
    }
  }
  
  [mDelegate updateLocationFields:urlSpec ignoreTyping:NO];

  // see if someone wants to replace the main view
  [self checkForCustomViewOnLoad:urlSpec];
}

- (void)onStatusChange:(NSString*)aStatusString
{
  [mDelegate updateStatus:aStatusString];
}

//
// onSecurityStateChange:
//
// Update the lock to the appropriate icon to match what necko is telling us, but
// only if we own the UI. If we're not the primary browser, we have no business
// mucking with the lock icon.
//
- (void)onSecurityStateChange:(unsigned long)newState
{
  mSecureState = newState;
  [mDelegate showSecurityState:mSecureState];
}

- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type 
{
  NSString* newStatus = nil;
  
  if (type == NSStatusTypeScriptDefault)
  {
    [mDefaultStatusString autorelease];
    mDefaultStatusString = [statusString retain];
  }
  else if (!statusString)
  {
    newStatus = (mDefaultStatusString) ? mDefaultStatusString : mLoadingStatusString;
  }
  else
  {
    newStatus = statusString;
  }
  
  if (newStatus)
    [mDelegate updateStatus:newStatus];
}

- (NSString *)title 
{
  return mTitle;
}

// this should only be called from the CHBrowserListener
- (void)setTitle:(NSString *)title
{
  [self setTabTitle:title windowTitle:title];
}

- (void)setTabTitle:(NSString*)tabTitle windowTitle:(NSString*)windowTitle
{
  NSString* curURL = [self getCurrentURI];

  [mTabTitle autorelease];
  mTabTitle  = [[self displayTitleForPageURL:curURL title:tabTitle] retain];
  
  [mTitle autorelease];
  
  NSString* newWindowTitle = [self displayTitleForPageURL:curURL title:windowTitle];
  if (mOffline)
    newWindowTitle = [NSString stringWithFormat:NSLocalizedString(@"OfflineTitleFormat", @""), newWindowTitle];
  mTitle = [newWindowTitle retain];
  
  [mDelegate updateWindowTitle:[mTitle stringByTruncatingTo:80 at:kTruncateAtEnd]];
  
  // Always set the tab.
  [mTabItem setLabel:mTabTitle];		// tab titles get truncated when setting them to tabs
}

- (NSString*)displayTitleForPageURL:(NSString*)inURL title:(NSString*)inTitle
{
  NSString* viewSourcePrefix = @"view-source:";
  if ([inURL hasPrefix:viewSourcePrefix])
    return [NSString stringWithFormat:NSLocalizedString(@"SourceOf", @""), [inURL substringFromIndex:[viewSourcePrefix length]]];

  if ([inTitle length] > 0)
    return inTitle;

  if (![inURL isEqualToString:@"about:blank"])
  {
    if ([inURL hasPrefix:@"file://"])
      return [inURL lastPathComponent];

    return inURL;
  }

  return NSLocalizedString(@"UntitledPageTitle", @"");
}

- (void)updateOfflineStatus
{
  nsCOMPtr<nsIIOService> ioService(do_GetService("@mozilla.org/network/io-service;1"));
  if (!ioService)
      return;
  PRBool offline = PR_FALSE;
  ioService->GetOffline(&offline);
  mOffline = offline;
}

//
// onShowTooltip:where:withText
//
// Unfortunately, we can't use cocoa's apis here because they rely on setting a region
// and waiting. We already have waited and we just want to display the tooltip, so we
// drop down to the Carbon help manager routines.
//
// |where| is relative to the browser view in QD coordinates (top left is (0,0)) 
// and must be converted to global QD coordinates for the carbon help manager.
//
- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text
{
  // if this tooltip originates from a tab that is (now) in the background,
  // or a background window, don't show it.
  if (![self window] || ![[self window] isMainWindow])
    return;

  NSPoint point = [[self window] convertBaseToScreen:[self convertPoint: where toView:nil]];
  [mToolTip showToolTipAtPoint: point withString: text overWindow:mWindow];
}

- (void)onHideTooltip
{
  [mToolTip closeToolTip];
}

//
// - onPopupBlocked:fromSite:
//
// Called when gecko blocks a popup, telling us who it came from. Keep track
// of the blocked URI so we can allow the user to unblock the site later. This
// doesn't show the blocked popup view, we wait until the page finishes loading
// to do that.
//
- (void)onPopupBlocked:(nsIURI*)inURIBlocked fromSite:(nsIURI*)inSite
{
  // lazily instantiate.
  if (!mBlockedSites)
    NS_NewISupportsArray(&mBlockedSites);
  if (mBlockedSites) {
    mBlockedSites->AppendElement(inSite);
    [mDelegate showPopupBlocked:YES];
  }
}

// Called when a "shortcut icon" link element is noticed
- (void)onFoundShortcutIcon:(NSString*)inIconURI
{
  BOOL useSiteIcons = [[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL];
  if (!useSiteIcons)
    return;
  
  if ([inIconURI length] > 0)
  {
    // if the favicon uri has changed, fire off favicon load. When it completes, our
    // imageLoadedNotification selector gets called.
    if (![inIconURI isEqualToString:mSiteIconURI])
    {
      [[SiteIconProvider sharedFavoriteIconProvider] fetchFavoriteIconForPage:[self getCurrentURI]
                                                             withIconLocation:inIconURI
                                                                 allowNetwork:YES
                                                              notifyingClient:self];
    }
  }
}

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  // presumably this is only called on the primary tab
  [[mWindow delegate] onShowContextMenu:flags domEvent:aEvent domNode:aNode];
}

-(NSMenu*)getContextMenu
{
  return [[mWindow delegate] getContextMenu];
}

-(NSWindow*)getNativeWindow
{
  // use the view's window first
  NSWindow* viewsWindow = [self window];
  if (viewsWindow)
    return viewsWindow;

  return mWindow;
}

- (BOOL)shouldAcceptDragFromSource:(id)dragSource
{
  if ((dragSource == self) || (dragSource == mTabItem) || (dragSource  == [[mWindow delegate] proxyIconView]))
    return NO;
  
  if ([mTabItem isMemberOfClass:[BrowserTabViewItem class]] && (dragSource == [(BrowserTabViewItem*)mTabItem tabItemContentsView]))
    return NO;
  
  return YES;
}

//
// closeBrowserWindow
//
// Gecko wants us to close the browser associated with this gecko instance. However,
// we're just one tab in the window so we don't really have the power to do this.
// Let the window controller have final say.
// 
- (void)closeBrowserWindow
{
  [[mWindow delegate] closeBrowserWindow:self];
}


//
// willShowPrompt
//
// Called before a prompt is shown for the contained view
// 
- (void)willShowPrompt
{
  [[mWindow delegate] willShowPromptForBrowser:self];
}

//
// didDismissPrompt
//
// Called after a prompt is shown for the contained view
//
- (void)didDismissPrompt
{
  [[mWindow delegate] didDismissPromptForBrowser:self];
}


- (void)getTitle:(NSString **)outTitle andHref:(NSString**)outHrefString
{
  *outTitle = [self pageTitle];
  *outHrefString = [self getCurrentURI];
}

- (void)offlineModeChanged:(NSNotification*)aNotification
{
  [self updateOfflineStatus];

  // This is pretty broken, and unused. We'd need to do this title futzing
  // on every title change, not just now.
  // XXX localize me
  NSString* titleTrailer = mOffline ? @" [Working Offline]" : @" [Working Offline]";
  NSString* newWindowTitle = [mTitle stringByAppendingString:titleTrailer];

  [mDelegate updateWindowTitle:newWindowTitle];
}

//
// sizeBrowserTo
//
// Sizes window so that browser has dimensions given by |dimensions|
//
- (void)sizeBrowserTo:(NSSize)dimensions
{
  NSRect bounds = [self bounds];
  float dx = dimensions.width - bounds.size.width;
  float dy = dimensions.height - bounds.size.height;

  NSRect frame = [[self window] frame];
  NSPoint topLeft = NSMakePoint(NSMinX(frame), NSMaxY(frame));
  frame.size.width += dx;
  frame.size.height += dy;
  
  // if we just call setFrame, it will change the top-left corner of the
  // window as it pulls the extra space from the top and right sides of the window,
  // which is not at all what the website desired. We must preserve
  // topleft of the window and reset it after we resize.
  [[self window] setFrame:frame display:YES];
  [[self window] setFrameTopLeftPoint:topLeft];
}

- (CHBrowserView*)createBrowserWindow:(unsigned int)aMask
{
  BOOL showBlocker = [[PreferenceManager sharedInstance] getBooleanPref:"browser.popups.showPopupBlocker" withSuccess:NULL];

  if (showBlocker) {
    nsCOMPtr<nsIDOMWindow> domWindow = [mBrowserView getContentWindow];
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(domWindow));
    if (piWindow->IsLoadingOrRunningTimeout()) {
      // A popup is being opened while the page is currently loading.  Offer to block the
      // popup.
      nsAlertController* controller = CHBrowserService::GetAlertController();
      
      BOOL confirm = NO;
      NS_DURING
        confirm = [controller confirm:[self window]
                                title:NSLocalizedString(@"PopupBlockTitle",@"")
                                 text:[NSString stringWithFormat:NSLocalizedString(@"PopupBlockMsg", @""), NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)]];

        // This is a one-time dialog.
        [[PreferenceManager sharedInstance] setPref:"browser.popups.showPopupBlocker" toBoolean:NO];
      NS_HANDLER
      NS_ENDHANDLER
      
      if (confirm)
        [[PreferenceManager sharedInstance] setPref:"dom.disable_open_during_load" toBoolean:YES];

      [[PreferenceManager sharedInstance] savePrefsFile]; // really necessary?

      if (confirm)
        return nil;
    }
  }

  BrowserWindowController* controller = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  [controller setChromeMask: aMask];
  [controller disableAutosave]; // The Web page opened this window, so we don't ever use its settings.
  [controller disableLoadPage]; // don't load about:blank initially since this is a script-opened window
  
  [controller window];		// force window load. The window gets made visible by CHBrowserListener::SetVisibility
  
  [[controller getBrowserWrapper] setPendingActive: YES];
  return [[controller getBrowserWrapper] getBrowserView];
}


//
// -reuseExistingBrowserWindow:
//
// Check the exact value of the "single-window mode" pref and if it's set to
// reuse the same view, return it. If it's set to create a new tab, do that
// and return that tab's view.
//
- (CHBrowserView*)reuseExistingBrowserWindow:(unsigned int)aMask
{
  CHBrowserView* viewToUse = mBrowserView;
  int openNewWindow = [[PreferenceManager sharedInstance] getIntPref:"browser.link.open_newwindow" withSuccess:NULL];
  if (openNewWindow == nsIBrowserDOMWindow::OPEN_NEWTAB) {
    // we decide whether or not to open the new tab in the background based on
    // if we're the fg tab. If we are, we assume the user wants to see the new tab
    // because it's contextually relevat. If this tab is in the bg, the user doesn't
    // want to be bothered with a bg tab throwing things up in their face. We know
    // we're in the bg if our delegate is nil.
    viewToUse = [mCreateDelegate createNewTabBrowser:(mDelegate == nil)];
  }

  return viewToUse;
}

//
// -shouldReuseExistingWindow
//
// Checks the pref to see if we want to reuse the same window (either in a new tab
// or re-use the same browser view) when loading a URL requesting a new window
//
- (BOOL)shouldReuseExistingWindow
{
  int openNewWindow = [[PreferenceManager sharedInstance] getIntPref:"browser.link.open_newwindow" withSuccess:NULL];
  BOOL shouldReuse = (openNewWindow == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW ||
                      openNewWindow == nsIBrowserDOMWindow::OPEN_NEWTAB);
  return shouldReuse;
}

- (CHBrowserView*)getBrowserView
{
  return mBrowserView;
}

- (void)setPendingActive:(BOOL)active
{
  mActivateOnLoad = active;
}

- (void)setSiteIconImage:(NSImage*)inSiteIcon
{
  [mSiteIconImage autorelease];
  mSiteIconImage = [inSiteIcon retain];
}

- (void)setSiteIconURI:(NSString*)inSiteIconURI
{
  [mSiteIconURI autorelease];
  mSiteIconURI = [inSiteIconURI retain];
}

// A nil inSiteIcon image indicates that we should use the default icon
- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI loadError:(BOOL)inLoadError
{
  BOOL     resetTabIcon     = NO;
  BOOL     tabIconDraggable = YES;
  NSImage* siteIcon         = inSiteIcon;
  
  if (![mSiteIconURI isEqualToString:inSiteIconURI] || inLoadError)
  {
    if (!siteIcon)
    {
      if (inLoadError)
        siteIcon = [NSImage imageNamed:@"brokenbookmark_icon"];   // it should have its own image
      else
        siteIcon = [NSImage imageNamed:@"globe_ico"];
    }

    if ([inSiteIconURI isEqualToString:@"about:blank"])
      tabIconDraggable = NO;

    [self setSiteIconImage:siteIcon];
    [self setSiteIconURI:inSiteIconURI];
  
    // update the proxy icon
    [mDelegate updateSiteIcons:mSiteIconImage ignoreTyping:NO];
      
    resetTabIcon = YES;
  }

  // update the tab icon
  if ([mTabItem isMemberOfClass:[BrowserTabViewItem class]])
  {
    BrowserTabViewItem* tabItem = (BrowserTabViewItem*)mTabItem;
    if (resetTabIcon || ![tabItem tabIcon])
      [tabItem setTabIcon:mSiteIconImage isDraggable:tabIconDraggable];
  }
}

- (void)registerNotificationListener
{
  [[NSNotificationCenter defaultCenter] addObserver:	self
                                        selector:     @selector(imageLoadedNotification:)
                                        name:         SiteIconLoadNotificationName
                                        object:				self];

}

// called when [[SiteIconProvider sharedFavoriteIconProvider] fetchFavoriteIconForPage:...] completes
- (void)imageLoadedNotification:(NSNotification*)notification
{
  NSDictionary* userInfo = [notification userInfo];
  if (userInfo)
  {
  	NSImage*  iconImage     = [userInfo objectForKey:SiteIconLoadImageKey];
    NSString* siteIconURI   = [userInfo objectForKey:SiteIconLoadURIKey];
    NSString* pageURI       = [userInfo objectForKey:SiteIconLoadUserDataKey];
    
    if (iconImage == nil)
      siteIconURI = @"";	// go back to default image
  
    if ([pageURI isEqualToString:[[self getBrowserView] getCurrentURI]]) // make sure it's for the current page
      [self updateSiteIconImage:iconImage withURI:siteIconURI loadError:NO];
  }
}


//
// -isEmpty
//
// YES if the page currently loaded in the browser view is "about:blank", NO otherwise
//
- (BOOL) isEmpty
{
  return [[[self getBrowserView] getCurrentURI] isEqualToString:@"about:blank"];
}


- (IBAction)reloadWithNewCharset:(NSString*)charset
{
  [[self getBrowserView] reloadWithNewCharset:charset];
}

- (NSString*)currentCharset
{
  return [[self getBrowserView] currentCharset];
}

- (void)getBlockedSites:(nsISupportsArray**)outSites
{
  if ( !outSites )
    return;
  *outSites = mBlockedSites;
  NS_IF_ADDREF(*outSites);
}

//
// -configurePopupBlocking:
//
// Called when the user clicks on the "configure" button in the blocked popup view.
// Sends the msg along to our UI delegate so they can handle it
//
- (IBAction)configurePopupBlocking:(id)sender
{
  [mDelegate configurePopupBlocking];
}

//
// -unblockPopupSites:
//
// Called when the user clicks on the "unblock" button in the blocked popup view.
// Sends the msg along with the list of sites whose popups we just blocked to our UI
// delegate so they can handle it. This also removes the blocked popup UI from
// the current window.
//
- (IBAction)unblockPopupSites:(id)sender
{
  NS_ASSERTION([self popupsBlocked], "no popups to unblock!");
  if ([self popupsBlocked]) {
    [mDelegate unblockAllPopupSites:mBlockedSites];
    [self removeBlockedPopupViewAndDisplay];
  }
}

//
// -removeBlockedPopupViewAndDisplay
//
// If we're showing the blocked popup view, this removes it and resizes the
// browser view to again take up all the space. Causes a full redraw of our
// view.
//
- (void)removeBlockedPopupViewAndDisplay
{
  if (mBlockedPopupView) {
    [mBlockedPopupView removeFromSuperview];
    mBlockedPopupView = nil;
    [self setFrame:[self frame] resizingBrowserViewIfHidden:YES];
    [self display];
  }
}

- (IBAction)hideBlockedPopupView:(id)sender
{
  [self removeBlockedPopupViewAndDisplay];
}

@end

#pragma mark -

@implementation InformationPanel

//
// -drawRect:
//
// Draw a background color and shadowed border in addition to the contents.
//
- (void)drawRect:(NSRect)aRect
{
  // draw background color
  [[NSColor colorWithCalibratedRed:1.0 green:0.9 blue:0.58 alpha:1.0] set];
  NSRectFill([self frame]);
  
  // draw shadowed border
  [[NSColor controlShadowColor] set];
  NSRectFill(NSMakeRect(aRect.origin.x, 0.0, aRect.size.width, 1.0));

  // Call our base class method to paint contents
  [super drawRect: aRect];
}

@end
