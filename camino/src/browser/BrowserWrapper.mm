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

#import "PreferenceManager.h"
#import "BrowserWrapper.h"
#import "BrowserWindowController.h"
#import "BookmarksClient.h"
#import "SiteIconProvider.h"
#import "BrowserTabViewItem.h"
#import "ToolTip.h"
#import "PageProxyIcon.h"
#import "KeychainService.h"
#import "AutoCompleteTextField.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "ContentClickListener.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserSetup.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIChromeEventHandler.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "CHBrowserService.h"
#include "nsIWebProgressListener.h"

#include <QuickDraw.h>

static const char* ioServiceContractID = "@mozilla.org/network/io-service;1";

const NSString* kOfflineNotificationName = @"offlineModeChanged";

@interface BrowserWrapper(Private)

- (void)setPendingActive:(BOOL)active;
- (void)registerNotificationListener;

- (void)setSiteIconImage:(NSImage*)inSiteIcon;
- (void)setSiteIconURI:(NSString*)inSiteIconURI;

- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI;

- (void)setTabTitle:(NSString*)tabTitle windowTitle:(NSString*)windowTitle;

@end

@implementation BrowserWrapper

- (id)initWithTab:(NSTabViewItem*)aTab andWindow:(NSWindow*)aWindow
{
  mTabItem = aTab;
  mWindow = aWindow;
  return [self initWithFrame: NSZeroRect];
}

//
// initWithFrame:  (designated initializer)
//
// Create a Gecko browser view and hook everything up to the UI
//
- (id)initWithFrame:(NSRect)frameRect
{
  if ( (self = [super initWithFrame: frameRect]) ) {
    mBrowserView = [[[CHBrowserView alloc] initWithFrame:[self bounds] andWindow: [self getNativeWindow]] autorelease];
    [self addSubview: mBrowserView];
    [mBrowserView setContainer:self];
    [mBrowserView addListener:self];
    [[KeychainService instance] addListenerToView:mBrowserView];
    mIsBusy = NO;
    mListenersAttached = NO;
    mSecureState = nsIWebProgressListener::STATE_IS_INSECURE;
    mProgress = 0.0;
    mBlockedSites = nsnull;

    BOOL gotPref;
    BOOL pluginsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:"chimera.enable_plugins" withSuccess:&gotPref];
    if (gotPref && !pluginsEnabled)
      [mBrowserView setProperty:nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS toValue:PR_FALSE];
    
    mToolTip = [[ToolTip alloc] init];

    //[self setSiteIconImage:[NSImage imageNamed:@"globe_ico"]];
    //[self setSiteIconURI: [NSString string]];
    
    mDefaultStatusString = nil;
    mLoadingStatusString = nil;

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
  
  [super dealloc];
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
  mIsPrimary = NO;
  mWindowController = nil;
}

- (IBAction)load:(id)sender
{
  [mBrowserView loadURI:[mUrlbar stringValue] referrer:nil flags:NSLoadFlagsNone];
}

-(void)disconnectView
{
  mUrlbar = nil;
  mStatus = nil;
  mIsPrimary = NO;
  [[NSNotificationCenter defaultCenter] removeObserver:self name:kOfflineNotificationName object:nil];

  [mBrowserView setActive: NO];
}

-(void)setTab: (NSTabViewItem*)tab
{
  mTabItem = tab;
}

-(NSTabViewItem*)tab
{
  return mTabItem;
}

-(void)makePrimaryBrowserView: (id)aUrlbar status: (id)aStatus
         windowController: (BrowserWindowController*)aWindowController
{
  mUrlbar = aUrlbar;
  mStatus = aStatus;
  mWindowController = aWindowController;

  if (mIsBusy)
  {
    [mWindowController startThrobber];    
    [mWindowController showProgressIndicator];
    
    if (mProgress > 0.0)
    {
      [[mWindowController progressIndicator] setIndeterminate:NO];
      [[mWindowController progressIndicator] setDoubleValue:mProgress];
    }
    else
    {
      [[mWindowController progressIndicator] setIndeterminate:YES];
      [[mWindowController progressIndicator] startAnimation:self];
    }
  }
  else
  {
    [mWindowController stopThrobber];    
    [mWindowController hideProgressIndicator];
  
    [mDefaultStatusString autorelease];
    mDefaultStatusString = nil;
    [mLoadingStatusString autorelease];
    mLoadingStatusString = [NSLocalizedString(@"DocumentDone", @"") retain];
  }

  [mStatus setStringValue:mLoadingStatusString];
  
  mIsPrimary = YES;

  // update the global lock icon to the current state of this browser. We need
  // to do this after we set |mIsPrimary|.
  [self onSecurityStateChange:mSecureState];

  // update the window's title. 
  [self setTabTitle:mTabTitle windowTitle:mTitle];

  if ([[self window] isKeyWindow] && ![mUrlbar userHasTyped])
    [mBrowserView setActive: YES];
  
  nsCOMPtr<nsIIOService> ioService(do_GetService(ioServiceContractID));
  if (!ioService)
    return;
  PRBool offline = PR_FALSE;
  ioService->GetOffline(&offline);
  mOffline = offline;
  
  if (mWindowController) // Only register if we're the content area.
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(offlineModeChanged:)
        name:kOfflineNotificationName
        object:nil];

  // update the blocked popup indicator
  [mWindowController showPopupBlocked:(mBlockedSites != nil)];
        
  // Update the URL bar, but only if the user hasn't put something of their
  // own in there.
  if (![mUrlbar userHasTyped]) {
    [mWindowController updateLocationFields:[self getCurrentURLSpec]];
    [mWindowController updateSiteIcons:mSiteIconImage];
  }
  
  if (mWindowController && !mListenersAttached)
  {
    mListenersAttached = YES;
    
    // We need to hook up our click and context menu listeners.
    ContentClickListener* clickListener = new ContentClickListener(mWindowController);
    if (!clickListener)
      return;
    
    nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([[self getBrowserView] getContentWindow]);
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(contentWindow));
    nsIChromeEventHandler *chromeHandler = piWindow->GetChromeEventHandler();
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(chromeHandler));
    if ( rec )
      rec->AddEventListenerByIID(clickListener, NS_GET_IID(nsIDOMMouseListener));
  }
}

-(NSString*)getCurrentURLSpec
{
  return [mBrowserView getCurrentURLSpec];
}

- (void)awakeFromNib
{
}

- (void)setFrame:(NSRect)frameRect
{
  [super setFrame:frameRect];

  // Only resize our browser view if we are visible.  If we're hidden, the frame
  // will get reset when we get placed back into the view hierarchy anyway.  This
  // enhancement keeps resizing in a window with many tabs from being slow.
  if ([self window]) {
    NSRect bounds = [self bounds];
    [mBrowserView setFrame:bounds];
  }
}

-(BOOL)isBusy
{
  return mIsBusy;
}

- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags activate:(BOOL)activate
{
  // blast it into the urlbar immediately so that we know what we're 
  // trying to load, even if it doesn't work
  if (mIsPrimary)
    [mWindowController updateLocationFields:urlSpec];

  mActivateOnLoad = activate;
  [mBrowserView loadURI:urlSpec referrer:referrer flags:flags];
}

- (void)onLoadingStarted 
{
  if (mDefaultStatusString) {
    [mDefaultStatusString autorelease];
    mDefaultStatusString = nil;
  }

  [mWindowController ensureBrowserVisible:self];
  [mWindowController showProgressIndicator];
  [[mWindowController progressIndicator] setIndeterminate:YES];
  [[mWindowController progressIndicator] startAnimation:self];

  [mLoadingStatusString autorelease];
  mLoadingStatusString = [NSLocalizedString(@"TabLoading", @"") retain];
  [mStatus setStringValue:mLoadingStatusString];
  
  [(BrowserTabViewItem*)mTabItem startLoadAnimation];

  NS_IF_RELEASE(mBlockedSites);
  [mWindowController showPopupBlocked:NO];
  
  mProgress = 0.0;
  mIsBusy = YES;
  
  [mTabTitle autorelease];
  mTabTitle = [mLoadingStatusString retain];
  [mTabItem setLabel:mTabTitle];

  if (mWindowController)
    [mWindowController loadingStarted];
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  if (mActivateOnLoad) {
    // if we're the front/key window, focus the content area. If we're not,
    // set gecko as the first responder so that it will be activated when
    // the window is focused. If the user is typing in the urlBar, however,
    // don't mess with the focus at all.
    if ( ![mUrlbar userHasTyped] ) {
      if ( [[mBrowserView window] isKeyWindow] )
        [mBrowserView setActive:YES];
      else
        [[mBrowserView window] makeFirstResponder:mBrowserView];
    }
    mActivateOnLoad = NO;
  }
  
  [[mWindowController progressIndicator] setIndeterminate:YES];
  [[mWindowController progressIndicator] stopAnimation:self];
  [mWindowController hideProgressIndicator];

  [mLoadingStatusString autorelease];
  mLoadingStatusString = [NSLocalizedString(@"DocumentDone", @"") retain];
  if (mDefaultStatusString) {
    [mStatus setStringValue:mDefaultStatusString];
  }
  else {
    [mStatus setStringValue:mLoadingStatusString];
  }
  
  [(BrowserTabViewItem*)mTabItem stopLoadAnimation];

  mProgress = 1.0;
  mIsBusy = NO;

  if (mWindowController)
    [mWindowController loadingDone];
  // send a little love to the bookmarks
  NSString *urlString = nil;
  NSString *titleString = nil;
  [self getTitle:&titleString andHref:&urlString];
  NSDictionary *userInfo = [NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedInt:0] forKey:URLLoadSuccessKey];
  if (urlString && ![urlString isEqualToString:@"about:blank"]) {
    NSNotification *note = [NSNotification notificationWithName:URLLoadNotification object:urlString userInfo:userInfo];
    [[NSNotificationQueue defaultQueue] enqueueNotification:note postingStyle:NSPostWhenIdle];
  }
}

- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes 
{
  if (maxBytes > 0)
  {
    mProgress = ((double)currentBytes / (double)maxBytes) * 100.0;
    if (mIsPrimary)
    {
      BOOL isIndeterminate = [[mWindowController progressIndicator] isIndeterminate];
      if (isIndeterminate)
        [[mWindowController progressIndicator] setIndeterminate:NO];
      [[mWindowController progressIndicator] setDoubleValue:mProgress];
    }
  }
}

- (void)onLocationChange:(NSString*)urlSpec
{
  BOOL useSiteIcons = [[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.site_icons" withSuccess:NULL];
  BOOL siteIconLoadInitiated = NO;
  
  SiteIconProvider* faviconProvider = [SiteIconProvider sharedFavoriteIconProvider];
  NSString* faviconURI = [SiteIconProvider faviconLocationStringFromURI:urlSpec];

  if (useSiteIcons && [faviconURI length] > 0)
  {
    // if the favicon uri has changed, fire off favicon load. When it completes, our
    // imageLoadedNotification selector gets called.
    if (![faviconURI isEqualToString:mSiteIconURI])
      siteIconLoadInitiated = [faviconProvider loadFavoriteIcon:self forURI:urlSpec allowNetwork:YES];
  }
  else
  {
    if ([urlSpec isEqualToString:@"about:blank"])
      faviconURI = urlSpec;
    else
    	faviconURI = @"";
  }

  if (!siteIconLoadInitiated)
    [self updateSiteIconImage:nil withURI:faviconURI];
  
  // if the user has started typing something, don't destroy it
  if (mIsPrimary && ![mUrlbar userHasTyped])
    [mWindowController updateLocationFields:urlSpec];  
}

- (void)onStatusChange:(NSString*)aStatusString
{
  if (![[mStatus stringValue] isEqualToString:aStatusString])
  {
    [mStatus setStringValue: aStatusString];
    //[mStatus displayIfNeeded];  // XXX expensive; slows page load
  }
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
  if ( mIsPrimary )
    [mWindowController updateLock:newState];
}

- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type 
{
  NSString* newStatus = nil;
  
  if (type == NSStatusTypeScriptDefault)
  {
    [mDefaultStatusString autorelease];
    mDefaultStatusString = statusString;
    [mDefaultStatusString retain];
  }
  else if (!statusString)
  {
    newStatus = (mDefaultStatusString) ? mDefaultStatusString : mLoadingStatusString;
  }
  else
  {
    newStatus = statusString;
  }
  
  if (newStatus && ![[mStatus stringValue] isEqualToString:newStatus])
  {
    [mStatus setStringValue:newStatus];
    //[mStatus displayIfNeeded];      // force an immediate display. This works around some issues
                                    // where cocoa unions update rects in the content and chrome,
                                    // causing slow updating (bug 2194819).
  }
}

- (NSString *)title 
{
  return mTitle;
}

- (void)setTitle:(NSString *)title
{
  if ([title length] == 0)
  {
    // if we get a blank title that is not for "about:blank", use the url,
    // or filename
    NSString* curURL = [self getCurrentURLSpec];
    if (![curURL isEqualToString:@"about:blank"])
    {
      if ([curURL hasPrefix:@"file://"])
        title = [curURL lastPathComponent];
      else
        title = curURL;
    }
  }

  [self setTabTitle:title windowTitle:title];
}

- (void)setTabTitle:(NSString*)tabTitle windowTitle:(NSString*)windowTitle
{
  const short kViewSourceLength = 12;         // strlen("view-source:")

  NSString* curURL = [self getCurrentURLSpec];
  [mTabTitle autorelease];
  if (tabTitle && [tabTitle length] > 0) {
    if ([curURL hasPrefix:@"view-source:"])
      mTabTitle = [NSString stringWithFormat:NSLocalizedString(@"SourceOf", @""), [curURL substringFromIndex:kViewSourceLength]];
    else	
      mTabTitle = tabTitle;
  } 
  else
    mTabTitle = NSLocalizedString(@"UntitledPageTitle", @"");
  [mTabTitle retain];
  
  [mTitle autorelease];
  if (windowTitle && [windowTitle length] > 0) {
    if ([curURL hasPrefix:@"view-source:"])
      mTitle = [NSString stringWithFormat:NSLocalizedString(@"SourceOf", @""), [curURL substringFromIndex:kViewSourceLength]];
    else
      mTitle = windowTitle;
  } 
  else
    mTitle = NSLocalizedString(@"UntitledPageTitle", @"");

  if (mOffline)
    mTitle = [NSString stringWithFormat:NSLocalizedString(@"OfflineTitleFormat", @""), mTitle];
  [mTitle retain];
  
  if (mIsPrimary)
    [[mWindowController window] setTitle:[mTitle stringByTruncatingTo:80 at:kTruncateAtEnd]];
  
  // Always set the tab.
  [mTabItem setLabel:mTabTitle];		// tab titles get truncated when setting them to tabs
}

- (BOOL)isFlipped
{
  return YES;
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
  NSPoint point = [[self window] convertBaseToScreen:[self convertPoint: where toView:nil]];
  [mToolTip showToolTipAtPoint: point withString: text];
}

- (void)onHideTooltip
{
  [mToolTip closeToolTip];
}

//
// - onPopupBlocked:fromSite:
//
// Called when gecko blocks a popup, telling us who it came from. Currently, we
// don't do anything with the blocked URI, but we have it just in case.
//
- (void)onPopupBlocked:(nsIURI*)inURIBlocked fromSite:(nsIURI*)inSite
{
  // lazily instantiate.
  if ( !mBlockedSites )
    NS_NewISupportsArray(&mBlockedSites);
  if ( mBlockedSites ) {
    mBlockedSites->AppendElement(inSite);
    [mWindowController showPopupBlocked:YES];
  }
}

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  [mWindowController onShowContextMenu: flags domEvent: aEvent domNode: aNode];
}

-(NSMenu*)getContextMenu
{
  return [mWindowController getContextMenu];
}

-(NSWindow*)getNativeWindow
{
  NSWindow* window = [self window];
  if (window)
    return window;

  if (mWindow)
    return mWindow;
  
  return nil;
}

- (BOOL)shouldAcceptDragFromSource:(id)dragSource
{
  if ((dragSource == self) || (dragSource == mTabItem) || (dragSource  == [mWindowController proxyIconView]))
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
  [mWindowController closeBrowserWindow:self];
}

- (void)getTitle:(NSString **)outTitle andHref:(NSString**)outHrefString
{
  *outTitle = *outHrefString = nil;

  nsCOMPtr<nsIDOMWindow> window = getter_AddRefs([mBrowserView getContentWindow]);
  if (!window) return;
  
  nsCOMPtr<nsIDOMDocument> htmlDoc;
  window->GetDocument(getter_AddRefs(htmlDoc));
  nsCOMPtr<nsIDocument> pageDoc(do_QueryInterface(htmlDoc));
  if (pageDoc)
  {
    nsIURI* url = pageDoc->GetDocumentURI();
    nsCAutoString spec;
    url->GetSpec(spec);
    *outHrefString = [NSString stringWithUTF8String:spec.get()];
  }

  nsAutoString titleString;
  nsCOMPtr<nsIDOMHTMLDocument> htmlDocument(do_QueryInterface(htmlDoc));
  if (htmlDocument)
    htmlDocument->GetTitle(titleString);
  if (!titleString.IsEmpty())
    *outTitle = [NSString stringWith_nsAString:titleString];
  else if (*outHrefString)
    *outTitle = [NSString stringWithString:*outHrefString];
}

- (void)offlineModeChanged: (NSNotification*)aNotification
{
    nsCOMPtr<nsIIOService> ioService(do_GetService(ioServiceContractID));
    if (!ioService)
        return;
    PRBool offline = PR_FALSE;
    ioService->GetOffline(&offline);
    mOffline = offline;
    
    if (mOffline) {
        NSString* newTitle = [[[mWindowController window] title] stringByAppendingString: @" [Working Offline]"];
        [[mWindowController window] setTitle: newTitle];
    }
    else {
        NSArray* titleItems = [[[mWindowController window] title] componentsSeparatedByString:@" [Working Offline]"];
        [[mWindowController window] setTitle: [titleItems objectAtIndex: 0]];
    }
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
  nsCOMPtr<nsIPrefBranch> pref(do_GetService("@mozilla.org/preferences-service;1"));
  if (!pref)
    return NS_OK; // Something bad happened if we can't get prefs.

  PRBool showBlocker;
  pref->GetBoolPref("browser.popups.showPopupBlocker", &showBlocker);

  if (showBlocker) {
    nsCOMPtr<nsIDOMWindow> domWindow = getter_AddRefs([mBrowserView getContentWindow]);
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(domWindow));
    if (piWindow->IsLoadingOrRunningTimeout()) {
      // A popup is being opened while the page is currently loading.  Offer to block the
      // popup.
      nsAlertController* controller = CHBrowserService::GetAlertController();
      BOOL confirm = [controller confirm: [self window] title: @"Unrequested Popup Detected"
                                text: [NSString stringWithFormat: NSLocalizedString(@"PopupBlockMsg", @""), NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)]];

      // This is a one-time dialog.
      pref->SetBoolPref("browser.popups.showPopupBlocker", PR_FALSE);
      
      if (confirm)
        pref->SetBoolPref("dom.disable_open_during_load", PR_TRUE);

      nsCOMPtr<nsIPrefService> prefService(do_QueryInterface(pref));
      prefService->SavePrefFile(nsnull);
      
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
// If inSiteIconURI is "about:blank", we don't show any icon
- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI
{
  BOOL     resetTabIcon     = NO;
  BOOL     tabIconDraggable = YES;
  NSImage* siteIcon         = inSiteIcon;
  
  if (![mSiteIconURI isEqualToString:inSiteIconURI])
  {
    if (!siteIcon)
    {
      if ([inSiteIconURI isEqualToString:@"about:blank"]) {
        siteIcon = [NSImage imageNamed:@"smallDocument"];
        tabIconDraggable = NO;
      } else
        siteIcon = [NSImage imageNamed:@"globe_ico"];
    }

    [self setSiteIconImage: siteIcon];
    [self setSiteIconURI:   inSiteIconURI];
  
    // update the proxy icon
    if (mIsPrimary)
      [mWindowController updateSiteIcons:mSiteIconImage];
      
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

// called when [[SiteIconProvider sharedFavoriteIconProvider] loadFavoriteIcon] completes
- (void)imageLoadedNotification:(NSNotification*)notification
{
  NSDictionary* userInfo = [notification userInfo];
  if (userInfo)
  {
  	NSImage*  iconImage     = [userInfo objectForKey:SiteIconLoadImageKey];
    NSString* siteIconURI   = [userInfo objectForKey:SiteIconLoadURIKey];
    
    // NSLog(@"BrowserWrapper imageLoadedNotification got image %@ and uri %@", iconImage, proxyImageURI);
    if (iconImage == nil)
      siteIconURI = @"";	// go back to default image
    
    [self updateSiteIconImage:iconImage withURI:siteIconURI];
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

@end
