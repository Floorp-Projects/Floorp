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
#import "ImageAdditions.h"

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
#import "RolloverImageButton.h"

#include "CHBrowserService.h"
#include "ContentClickListener.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#ifdef MOZILLA_1_8_BRANCH
#include "nsIArray.h"
#else
#include "nsIMutableArray.h"
#endif

#include "nsIArray.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserSetup.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsPIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIWebProgressListener.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIPermissionManager.h"

class nsIDOMPopupBlockedEvent;

// for camino.enable_plugins; needs to match string in WebFeatures.mm
static NSString* const kEnablePluginsChangedNotificationName = @"EnablePluginsChanged";

// types of status bar messages, in order of priority for showing to the user
enum {
  eStatusLinkTarget    = 0, // link mouseover info
  eStatusProgress      = 1, // loading progress
  eStatusScript        = 2, // javascript window.status
  eStatusScriptDefault = 3, // javascript window.defaultStatus
};

@interface BrowserWrapper(Private)

- (void)ensureContentClickListeners;

- (void)setPendingActive:(BOOL)active;
- (void)registerNotificationListener;

- (void)clearStatusStrings;

- (void)setSiteIconImage:(NSImage*)inSiteIcon;
- (void)setSiteIconURI:(NSString*)inSiteIconURI;

- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI loadError:(BOOL)inLoadError;

- (void)setPendingURI:(NSString*)inURI;

- (NSString*)displayTitleForPageURL:(NSString*)inURL title:(NSString*)inTitle;

- (void)updatePluginsEnabledState;

- (void)checkForCustomViewOnLoad:(NSString*)inURL;

- (BOOL)popupsAreBlacklistedForURL:(NSString*)inURL;
- (void)showPopupsWhitelistingSource:(BOOL)shouldWhitelist;
- (void)addBlockedPopupViewAndDisplay;
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
    mFeedList = nil;

    [self updatePluginsEnabledState];

    mToolTip = [[ToolTip alloc] init];

    //[self setSiteIconImage:[NSImage imageNamed:@"globe_ico"]];
    //[self setSiteIconURI: [NSString string]];

    // prefill with a null value for each of the four types of status strings
    mStatusStrings = [[NSMutableArray alloc] initWithObjects:[NSNull null], [NSNull null],
                                                             [NSNull null], [NSNull null], nil];

    mDisplayTitle = [[NSString alloc] init];
    
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
  [mStatusStrings release];

  [mToolTip release];
  [mDisplayTitle release];
  [mPendingURI release];
  
  NS_IF_RELEASE(mBlockedPopups);
  
  [mFeedList release];
  
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

-(NSString*)pendingURI
{
  return mPendingURI;
}

-(NSString*)currentURI
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
  if ([self window] || inResizeBrowser) {
    NSRect bounds = [self bounds];
    if (mBlockedPopupView) {
      // First resize the width of blockedPopupView to match this view.
      // The blockedPopupView will, during its setFrame method, wrap information 
      // text if necessary and adjust its own height to enclose that text.
      // Then find out the actual (possibly adjusted) height of blockedPopupView 
      // and resize the browser view accordingly.
      // Recall that we're flipped, so the origin is the top left.

      NSRect popupBlockFrame = [mBlockedPopupView frame];
      popupBlockFrame.origin = NSZeroPoint;
      popupBlockFrame.size.width = bounds.size.width;
      [mBlockedPopupView setFrame:popupBlockFrame];

      NSRect blockedPopupViewFrameAfterResized = [mBlockedPopupView frame];
      NSRect browserFrame = [mBrowserView frame];
      browserFrame.origin.y = blockedPopupViewFrameAfterResized.size.height;
      browserFrame.size.width = bounds.size.width;
      browserFrame.size.height = bounds.size.height-blockedPopupViewFrameAfterResized.size.height;

      [mBrowserView setFrame:browserFrame];
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

- (NSString*)displayTitle
{
  return mDisplayTitle;
}

- (NSString*)pageTitle
{
  return [mBrowserView pageTitle];
}

- (NSImage*)siteIcon
{
  return mSiteIconImage;
}

- (NSString*)statusString
{
  // Return the highest-priority status string that is set, or the empty string if none are set
  for (unsigned int i = 0; i < [mStatusStrings count]; ++i)
  {
    id status = [mStatusStrings objectAtIndex:i];
    if (status != [NSNull null])
      return status;
  }
  return @"";
}

- (float)loadingProgress
{
  return mProgress;
}

- (BOOL)popupsBlocked
{
  if (!mBlockedPopups) return NO;
  
  PRUint32 numBlocked = 0;
  mBlockedPopups->GetLength(&numBlocked);
  
  return (numBlocked > 0);
}

- (unsigned long)securityState
{
  return mSecureState;
}

- (BOOL)feedsDetected
{
	return (mFeedList && [mFeedList count] > 0);
}

- (void)loadURI:(NSString*)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags focusContent:(BOOL)focusContent allowPopups:(BOOL)inAllowPopups
{
  // blast it into the urlbar immediately so that we know what we're 
  // trying to load, even if it doesn't work
  [mDelegate updateLocationFields:urlSpec ignoreTyping:YES];
  
  [self setPendingURI:urlSpec];

  // if we're not the primary tab, make sure that the browser view is 
  // the correct size when loading a url so that if the url is a relative
  // anchor, which will cause a scroll to the anchor on load, the scroll
  // position isn't messed up when we finally display the tab.
  if (mDelegate == nil)
  {
    NSRect tabContentRect = [[[mWindow delegate] getTabBrowser] contentRect];
    [self setFrame:tabContentRect resizingBrowserViewIfHidden:YES];
  }

  if ([[PreferenceManager sharedInstance] getBooleanPref:"keyword.enabled" withSuccess:NULL])
    flags |= NSLoadFlagsAllowThirdPartyFixup;

  [self setPendingActive:focusContent];
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
    nsPIDOMEventTarget *chromeHandler = piWindow->GetChromeEventHandler();
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(chromeHandler));
    if ( rec )
      rec->AddEventListenerByIID(clickListener, NS_GET_IID(nsIDOMMouseListener));
  }
}

- (void)didBecomeActiveBrowser
{
  [self ensureContentClickListeners];
}

-(void)willResignActiveBrowser
{
  [mToolTip closeToolTip];

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
  [self clearStatusStrings];

  mProgress = 0.0;
  mIsBusy = YES;

  [mDelegate loadingStarted];
  [mDelegate setLoadingActive:YES];
  [mDelegate setLoadingProgress:mProgress];

  [mStatusStrings replaceObjectAtIndex:eStatusProgress withObject:NSLocalizedString(@"TabLoading", @"")];
  [mDelegate updateStatus:[self statusString]];

  [(BrowserTabViewItem*)mTabItem startLoadAnimation];
  
  [mDelegate showFeedDetected:NO];
  [mFeedList removeAllObjects];
  
  [mTabItem setLabel:NSLocalizedString(@"TabLoading", @"")];
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  [mDelegate loadingDone:mActivateOnLoad];
  mActivateOnLoad = NO;
  mIsBusy = NO;
  [self setPendingURI:nil];
  
  [mDelegate setLoadingActive:NO];

  [mStatusStrings replaceObjectAtIndex:eStatusProgress withObject:[NSNull null]];
  [mDelegate updateStatus:[self statusString]];

  [(BrowserTabViewItem*)mTabItem stopLoadAnimation];

  NSString *urlString = [self currentURI];
  NSString *titleString = [self pageTitle];
  
  // If we never got a page title, then the tab title will be stuck at "Loading..."
  // so be sure to set the title here
  NSString* tabTitle = [self displayTitleForPageURL:urlString title:titleString];
  [mTabItem setLabel:tabTitle];
  
  mProgress = 1.0;

  // tell the bookmarks when a url loaded.
  // note that this currently fires even when you go Back of Forward to the page,
  // so it's not a great way to count bookmark visits.
  if (urlString && ![urlString isEqualToString:@"about:blank"])
  {
    NSDictionary*   userInfo = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:succeeded] forKey:URLLoadSuccessKey];
    NSNotification* note     = [NSNotification notificationWithName:URLLoadNotification object:urlString userInfo:userInfo];
    [[NSNotificationQueue defaultQueue] enqueueNotification:note postingStyle:NSPostWhenIdle];
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
    if(mBlockedPopups)
      mBlockedPopups->Clear();
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
  [mStatusStrings replaceObjectAtIndex:eStatusProgress withObject:aStatusString];
  [mDelegate updateStatus:[self statusString]];
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
  int index;

  if (type == NSStatusTypeScriptDefault)
    index = eStatusScriptDefault;
  else if (type == NSStatusTypeScript)
    index = eStatusScript;
  else
    index = eStatusLinkTarget;

  [mStatusStrings replaceObjectAtIndex:index withObject:(statusString ? (id)statusString : (id)[NSNull null])];
  [mDelegate updateStatus:[self statusString]];
}

- (void)clearStatusStrings
{
  for (unsigned int i = 0; i < [mStatusStrings count]; ++i)
    [mStatusStrings replaceObjectAtIndex:i withObject:[NSNull null]];
}

- (NSString *)title
{
  return mDisplayTitle;
}

// this should only be called from the CHBrowserListener
- (void)setTitle:(NSString *)title
{
  [mDisplayTitle autorelease];
  mDisplayTitle = [[self displayTitleForPageURL:[self currentURI] title:title] retain];
  
  [mTabItem setLabel:mDisplayTitle];
  [mDelegate updateWindowTitle:mDisplayTitle];
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

- (void)updatePluginsEnabledState
{
  BOOL gotPref;
  BOOL pluginsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:"camino.enable_plugins" withSuccess:&gotPref];

  // If we can't get the pref, ensure we leave plugins enabled.
  [mBrowserView setProperty:nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS toValue:(gotPref ? pluginsEnabled : YES)];
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
// - onPopupBlocked:
//
// Called when gecko blocks a popup, telling us who it came from, the modifiers of the popup
// and more data that we'll need if the user wants to unblock the popup later.
//
- (void)onPopupBlocked:(nsIDOMPopupBlockedEvent*)eventData;
{
  // If popups from this site have been blacklisted, silently discard the event.
  if ([self popupsAreBlacklistedForURL:[self currentURI]])
    return;
  // lazily instantiate.
  if (!mBlockedPopups)
    CallCreateInstance(NS_ARRAY_CONTRACTID, &mBlockedPopups);
  if (mBlockedPopups) {
    mBlockedPopups->AppendElement((nsISupports*)eventData, PR_FALSE);
    [self addBlockedPopupViewAndDisplay];
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
      [[SiteIconProvider sharedFavoriteIconProvider] fetchFavoriteIconForPage:[self currentURI]
                                                             withIconLocation:inIconURI
                                                                 allowNetwork:YES
                                                              notifyingClient:self];
    }
  }
}

- (void)onFeedDetected:(NSString*)inFeedURI feedTitle:(NSString*)inFeedTitle
{
  // add the two in variables to a dictionary, then store in the feed array
  NSDictionary* feed = [NSDictionary dictionaryWithObjectsAndKeys:inFeedURI, @"feeduri", inFeedTitle, @"feedtitle", nil];
  
  if (!mFeedList)
    mFeedList = [[NSMutableArray alloc] init];
  
  [mFeedList addObject:feed];
  // notify the browser UI that a feed was found
  [mDelegate showFeedDetected:YES];
}

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  // presumably this is only called on the primary tab
  [[mWindow delegate] onShowContextMenu:flags domEvent:aEvent domNode:aNode];
}

-(NSMenu*)contextMenu
{
  return [[mWindow delegate] contextMenu];
}

-(NSWindow*)nativeWindow
{
  // use the view's window first
  NSWindow* viewsWindow = [self window];
  if (viewsWindow)
    return viewsWindow;

  return mWindow;
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

- (void)enablePluginsChanged:(NSNotification*)aNote
{
  [self updatePluginsEnabledState];
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

// Checks to see if we should allow window.open calls with specified size/position to open new windows (regardless of SWM)
- (int)respectWindowOpenCallsWithSizeAndPosition
{
  return ([[PreferenceManager sharedInstance] getIntPref:"browser.link.open_newwindow.restriction" withSuccess:NULL] == 2);
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

- (void)setPendingURI:(NSString*)inURI
{
  [mPendingURI autorelease];
  mPendingURI = [inURI retain];
}

- (void)registerNotificationListener
{
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(imageLoadedNotification:)
                                               name:SiteIconLoadNotificationName
                                             object:self];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(enablePluginsChanged:)
                                               name:kEnablePluginsChangedNotificationName
                                             object:nil];
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
  
    if ([pageURI isEqualToString:[self currentURI]]) // make sure it's for the current page
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
  return [[self currentURI] isEqualToString:@"about:blank"];
}

- (BOOL)isInternalURI
{
  NSString* currentURI = [self currentURI];
  return ([currentURI hasPrefix:@"about:"] || [currentURI hasPrefix:@"view-source:"]);
}

- (BOOL)canReload
{
  NSString* curURI = [[self currentURI] lowercaseString];
  return (![self isEmpty] &&
          !([curURI isEqualToString:@"about:bookmarks"] ||
            [curURI isEqualToString:@"about:history"] ||
            [curURI isEqualToString:@"about:config"]));
}

- (void)reload:(unsigned int)reloadFlags
{
  // Toss the favicon when force reloading
  if (reloadFlags == NSLoadFlagsBypassCacheAndProxy)
    [[SiteIconProvider sharedFavoriteIconProvider] removeImageForPageURL:[self currentURI]];

  [mBrowserView reload:reloadFlags];
}

- (IBAction)reloadWithNewCharset:(NSString*)charset
{
  [[self getBrowserView] reloadWithNewCharset:charset];
}

- (NSString*)currentCharset
{
  return [[self getBrowserView] currentCharset];
}

//
// -feedList:
//
// Return the list of feeds that were found on this page.
//
- (NSArray*)feedList
{
  return mFeedList;
}

#pragma mark -

- (BOOL)popupsAreBlacklistedForURL:(NSString*)inURL
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), [inURL UTF8String]);
  nsCOMPtr<nsIPermissionManager> pm(do_GetService(NS_PERMISSIONMANAGER_CONTRACTID));
  if (pm && uri) {
    PRUint32 permission;
    pm->TestPermission(uri, "popup", &permission);
    return (permission == nsIPermissionManager::DENY_ACTION);
  }
  return NO;
}

//
// -showPopups:
//
// Called when the user clicks on the "Allow Once" button in the blocked popup view.
// Shows the blocked popups without whitelisting the source page.
//
- (IBAction)showPopups:(id)sender
{
  [self showPopupsWhitelistingSource:NO];
}

//
// -unblockPopups:
//
// Called when the user clicks on the "Always Allow" button in the blocked popup view.
// Shows the blocked popups and whitelists the source page.
//
- (IBAction)unblockPopups:(id)sender
{
  [self showPopupsWhitelistingSource:YES];
}

//
// -blacklistPopups:
//
// Called when the user clicks on the "Never Allow" button in the blocked popup view.
// Adds the current site to the blacklist, and dismisses the blocked popup UI.
//
- (IBAction)blacklistPopups:(id)sender
{
  [mDelegate blacklistPopupsFromURL:[self currentURI]];
  [self removeBlockedPopupViewAndDisplay];
}

//
// -showPopupsWhitelistingSource:
//
// Private helper method to handle showing blocked popups.
// Sends the the list of popups we just blocked to our UI delegate so it can
// handle them. This also removes the blocked popup UI from the current window.
//
- (void)showPopupsWhitelistingSource:(BOOL)shouldWhitelist
{
  NS_ASSERTION([self popupsBlocked], "no popups to unblock!");
  if ([self popupsBlocked]) {
    nsCOMPtr<nsIArray> blockedSites = do_QueryInterface(mBlockedPopups);
    [mDelegate showBlockedPopups:blockedSites whitelistingSource:shouldWhitelist];
    [self removeBlockedPopupViewAndDisplay];
  }
}

//
// -addBlockedPopupViewAndDisplay
//
// Even if we're hidden, we ensure that the new view is in the view hierarchy
// and it will be resized when the current tab is eventually displayed.
//
- (void)addBlockedPopupViewAndDisplay
{
  if ([self popupsBlocked] && !mBlockedPopupView) {
    [NSBundle loadNibNamed:@"PopupBlockView" owner:self];
    
    NSString* currentHost = [[NSURL URLWithString:[self currentURI]] host];
    if (!currentHost)
      currentHost = NSLocalizedString(@"GenericHostString", nil);
    [mBlockedPopupLabel setStringValue:[NSString stringWithFormat:NSLocalizedString(@"PopupDisplayRequest", nil), currentHost]];
    [mBlockedPopupCloseButton setImage:[NSImage imageNamed:@"popup_close"]];
    [mBlockedPopupCloseButton setAlternateImage:[NSImage imageNamed:@"popup_close_pressed"]];
    [mBlockedPopupCloseButton setHoverImage:[NSImage imageNamed:@"popup_close_hover"]];
    
    [self addSubview:mBlockedPopupView];
    [self setFrame:[self frame] resizingBrowserViewIfHidden:YES];
    [self display];
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
    [mBlockedPopupView release]; // retain count of 1 from nib
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

// This value keeps the message text field from wrapping excessively.
#define kMessageTextMinWidth 70

@implementation InformationPanelView

- (void)awakeFromNib
{
  [self verticallyCenterAllSubviews];

  // Padding & strut length are required when setting the panel's frame.
  NSRect textFieldFrame = [mPopupBlockedMessageTextField frame];
  mVerticalPadding = [mPopupBlockedMessageTextField frame].origin.y;
  mMessageTextRightStrutLength = [self frame].size.width - NSMaxX(textFieldFrame);
}

//
// -setFrame:
// In addition to setting the panel's frame rectangle this method accounts
// for wrapping of the message text field in response to this new frame and
// adjusts to properly enclose the text, maintaining vertical padding.
//
- (void)setFrame:(NSRect)newPanelFrame
{
  NSRect existingPanelFrame = [self frame];
  NSRect textFieldFrame = [mPopupBlockedMessageTextField frame];

  // Resize the text field's width (based on its right strut).
  float currentStrutLength = newPanelFrame.size.width - NSMaxX(textFieldFrame);
  textFieldFrame.size.width -= mMessageTextRightStrutLength - currentStrutLength;

  // Enforce a minimum size for the text field.
  if (textFieldFrame.size.width < kMessageTextMinWidth)
    textFieldFrame.size.width = kMessageTextMinWidth;

  // Text field will wrap/resize when its new frame is applied.
  [mPopupBlockedMessageTextField setFrame:textFieldFrame];
  textFieldFrame = [mPopupBlockedMessageTextField frame];

  newPanelFrame.size.height = textFieldFrame.size.height + 2 * mVerticalPadding;
  [super setFrame:newPanelFrame];

  [self verticallyCenterAllSubviews];
}

- (void)verticallyCenterAllSubviews
{
  NSRect panelFrame = [self frame];

  NSEnumerator *subviewEnum = [[self subviews] objectEnumerator];
  NSView *currentSubview;

  while ((currentSubview = [subviewEnum nextObject])) {
    NSRect currentSubviewFrame = [currentSubview frame];
    // The panel's NSButtons draw incorrectly on non-integral pixel boundaries.
    float verticallyCenteredYLocation = (int)((panelFrame.size.height - currentSubviewFrame.size.height) / 2.0f);

    [currentSubview setFrameOrigin:NSMakePoint(currentSubviewFrame.origin.x, verticallyCenteredYLocation)];
  }
}

//
// CalculateShadingValues
//
// Callback function; Generates a color based upon
// the current interval (location) of the shading.
//
static void CalculateShadingValues(void *info, const float *in, float *out)
{
  static float beginTopHalf[4] =    { 0.364706f, 0.364706f, 0.364706f, 1.0f };
  static float endTopHalf[4] =      { 0.298039f, 0.298039f, 0.298039f, 1.0f };
  static float beginBottomHalf[4] = { 0.207843f, 0.207843f, 0.207843f, 1.0f };
  static float endBottomHalf[4] =   { 0.290196f, 0.290196f, 0.290196f, 1.0f };

  float *startColor;
  float *endColor;

  // The interval is the sole item in the input array.
  // It ranges from 0 - 1.0.
  float currentInterval = in[0];

  // Determine which shading to draw based upon the interval and adjust
  // that interval so each shading contains a full range of 0 - 1.0.
  if (currentInterval < 0.5f) {
    startColor = beginTopHalf;
    endColor = endTopHalf;
    currentInterval /= 0.5f;
  }
  else {
    startColor = beginBottomHalf;
    endColor = endBottomHalf;
    currentInterval = (currentInterval - 0.5f) / 0.5f;
  }

  // Using the interval, compute and set each color component (RGBa) output.
  for(int i = 0; i < 4; i++)
    out[i] = (1.0f - currentInterval) * startColor[i] + currentInterval * endColor[i];
}

//
// -drawRect:
//
// Draws a shading behind the view's contents.
//
- (void)drawRect:(NSRect)aRect
{
  struct CGFunctionCallbacks shadingCallback = { 0, &CalculateShadingValues, NULL };

  CGFunctionRef shadingFunction = CGFunctionCreate(NULL,              // void *info
                                                   1,                 // number of inputs
                                                   NULL,              // valid intervals of input values
                                                   4,                 // number of outputs (4 = RGBa)
                                                   NULL,              // valid intervals of output values
                                                   &shadingCallback); // pointer to callback function

  if (!shadingFunction) {
    NSLog(@"Failed to create a shading function.");
    return;
  }

  NSRect bounds = [self bounds];

  // Start/end at the top/bottom midpoint
  CGPoint startPoint = CGPointMake(NSMidX(bounds), NSMaxY(bounds));
  CGPoint endPoint = CGPointMake(NSMidX(bounds), NSMinY(bounds));

  // To preserve 10.3 compatibility, create a color space
  // using the CGColorSpaceCreateDeviceRGB function.

  // CGColorSpaceCreateDeviceRGB has two possible behaviors:
  // 1. On 10.3 or earlier it returns a device-dependent color space.
  // 2. On 10.4 or later it will map to a generic (and more accurate)
  //    device-independent color space.
  CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

  if (!colorspace) {
    NSLog(@"Failed to create a color space for the shading.");
    CGFunctionRelease(shadingFunction);
    return;
  }

  // Creates (but does not draw) the axial shading
  CGShadingRef shading = CGShadingCreateAxial(colorspace,       // CGColorSpaceRef colorspace
                                              startPoint,       // CGPoint start
                                              endPoint,         // CGPoint end
                                              shadingFunction,  // CGFunctionRef function
                                              false,            // bool extendStart
                                              false);           // bool extendEnd

  if (!shading) {
    NSLog(@"Failed to create the shading.");
    CGFunctionRelease(shadingFunction);
    CGColorSpaceRelease(colorspace);
    return;
  }

  CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

  CGContextDrawShading(context, shading);

  CGShadingRelease(shading);
  CGColorSpaceRelease(colorspace);
  CGFunctionRelease(shadingFunction);

  [super drawRect:aRect];
}

@end
