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

#import "CHPreferenceManager.h"
#import "CHBrowserWrapper.h"
#import "BrowserWindowController.h"
#import "BookmarksService.h"
#import "SiteIconProvider.h"
#import "BrowserTabViewItem.h"
#import "ToolTip.h"
#import "CHPageProxyIcon.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "ContentClickListener.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowser.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIChromeEventHandler.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsCocoaBrowserService.h"
#include "nsIWebProgressListener.h"

#include <QuickDraw.h>

#define DOCUMENT_DONE_STRING @"Document: Done"

static const char* ioServiceContractID = "@mozilla.org/network/io-service;1";

const NSString* kOfflineNotificationName = @"offlineModeChanged";

@interface CHBrowserWrapper(Private)

- (void)setPendingActive:(BOOL)active;
- (void)registerNotificationListener;

- (void)setSiteIconImage:(NSImage*)inSiteIcon;
- (void)setSiteIconURI:(NSString*)inSiteIconURI;

- (void)updateSiteIconImage:(NSImage*)inSiteIcon withURI:(NSString *)inSiteIconURI;

@end

@implementation CHBrowserWrapper

- (id)initWithTab:(NSTabViewItem*)aTab andWindow:(NSWindow*)aWindow
{
  mTabItem = aTab;
  mWindow = aWindow;
  mIsBookmarksImport = NO;
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
    mIsBusy = NO;
    mListenersAttached = NO;
    mSecureState = nsIWebProgressListener::STATE_IS_INSECURE;

    mToolTip = [[ToolTip alloc] init];

    //[self setSiteIconImage:[NSImage imageNamed:@"globe_ico"]];
    //[self setSiteIconURI: [NSString string]];
    
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

  [super dealloc];
}


-(void)windowClosed
{
  // Break the cycle.
  [mBrowserView destroyWebBrowser];
  [mBrowserView setContainer: nil];
  [mBrowserView removeListener: self];
}

- (IBAction)load:(id)sender
{
  [mBrowserView loadURI:[mUrlbar stringValue] referrer:nil flags:NSLoadFlagsNone];
}

-(void)disconnectView
{
  mUrlbar = nil;
  mStatus = nil;
  mProgress = nil;
  mProgressSuper = nil;
  mIsPrimary = NO;
  [[NSNotificationCenter defaultCenter] removeObserver:self name:kOfflineNotificationName object:nil];

  [mBrowserView setActive: NO];
}

-(void)setTab: (NSTabViewItem*)tab
{
  mTabItem = tab;
}

-(void)makePrimaryBrowserView: (id)aUrlbar status: (id)aStatus
        progress: (id)aProgress windowController: (BrowserWindowController*)aWindowController
{
  mUrlbar = aUrlbar;
  mStatus = aStatus;
  mProgress = aProgress;
  mProgressSuper = [aProgress superview];
  mWindowController = aWindowController;

  if (!mIsBusy)
    [mProgress removeFromSuperview];
  
  mDefaultStatusString = NULL;
  mLoadingStatusString = DOCUMENT_DONE_STRING;
  [mStatus setStringValue:mLoadingStatusString];
  
  mIsPrimary = YES;

  // update the global lock icon to the current state of this browser. We need
  // to do this after we set |mIsPrimary|.
  [self onSecurityStateChange:mSecureState];

  // update the window's title. 
  [self setTitle:mTitle];

  if ([[self window] isKeyWindow])
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
        
  // Update the URL bar.
  [mWindowController updateLocationFields:[self getCurrentURLSpec]];
  [mWindowController updateSiteIcons:mSiteIconImage];
  
  if (mWindowController && !mListenersAttached) {
    mListenersAttached = YES;
    
    // We need to hook up our click and context menu listeners.
    ContentClickListener* clickListener = new ContentClickListener(mWindowController);
    if (!clickListener)
      return;
    
    nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([[self getBrowserView] getContentWindow]);
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(contentWindow));
    nsCOMPtr<nsIChromeEventHandler> chromeHandler;
    piWindow->GetChromeEventHandler(getter_AddRefs(chromeHandler));
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(chromeHandler));
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
  mActivateOnLoad = activate;
  [mBrowserView loadURI:urlSpec referrer:referrer flags:flags];
}

- (void)onLoadingStarted 
{
  if (mDefaultStatusString) {
    [mDefaultStatusString release];
    mDefaultStatusString = NULL;
  }

  [mProgressSuper addSubview:mProgress];
  [mProgress setIndeterminate:YES];
  [mProgress startAnimation:self];

  mLoadingStatusString = NSLocalizedString(@"TabLoading", @"");
  [mStatus setStringValue:mLoadingStatusString];

  mIsBusy = YES;
  [mTabItem setLabel: NSLocalizedString(@"TabLoading", @"")];

  if (mWindowController) {
    [mWindowController updateToolbarItems];
    [mWindowController startThrobber];
  }
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  if (mActivateOnLoad) {
    [mBrowserView setActive:YES];
    mActivateOnLoad = NO;
  }
  
  [mProgress setIndeterminate:YES];
  [mProgress stopAnimation:self];
  [mProgress removeFromSuperview];

  mLoadingStatusString = DOCUMENT_DONE_STRING;
  if (mDefaultStatusString) {
    [mStatus setStringValue:mDefaultStatusString];
  }
  else {
    [mStatus setStringValue:mLoadingStatusString];
  }

  mIsBusy = NO;

  if (mIsBookmarksImport) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([mBrowserView getWebBrowser]);
    webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsIDOMDocument> domDocument;
      domWindow->GetDocument(getter_AddRefs(domDocument));
      nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(domDocument));
      if (htmlDoc)
        BookmarksService::ImportBookmarks(htmlDoc);
    }
    [self windowClosed];
    [self removeFromSuperview];
  }

  if (mWindowController) {
    [mWindowController updateToolbarItems];
    [mWindowController stopThrobber];
  }
}

- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes 
{
  if (maxBytes > 0) {
    BOOL isIndeterminate = [mProgress isIndeterminate];
    if (isIndeterminate) {
      [mProgress setIndeterminate:NO];
    }
    double val = ((double)currentBytes / (double)maxBytes) * 100.0;
    [mProgress setDoubleValue:val];
  }
}

- (void)onLocationChange:(NSString*)urlSpec
{
  BOOL useSiteIcons = [[CHPreferenceManager sharedInstance] getBooleanPref:"browser.chrome.site_icons" withSuccess:NULL];
  BOOL siteIconLoadInitiated = NO;
  
  SiteIconProvider* faviconProvider = [SiteIconProvider sharedFavoriteIconProvider];
  NSString* faviconURI = [SiteIconProvider faviconLocationStringFromURI:urlSpec];

  if (useSiteIcons && [faviconURI length] > 0)
  {
    // if the favicon uri has changed, fire off favicon load. When it completes, our
    // imageLoadedNotification selector gets called.
    if (![faviconURI isEqualToString:mSiteIconURI])
      siteIconLoadInitiated = [faviconProvider loadFavoriteIcon:self forURI:urlSpec withUserData:nil allowNetwork:YES];
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
  
  if (mIsPrimary)
    [mWindowController updateLocationFields:urlSpec];  
}

- (void)onStatusChange:(NSString*)aStatusString
{
  [mStatus setStringValue: aStatusString];
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
  if (type == NSStatusTypeScriptDefault) {
    if (mDefaultStatusString) {
      [mDefaultStatusString release];
    }
    mDefaultStatusString = statusString;
    if (mDefaultStatusString) {
      [mDefaultStatusString retain];
    }
  }
  else if (!statusString) {
    if (mDefaultStatusString) {
      [mStatus setStringValue:mDefaultStatusString];
    }
    else {
      [mStatus setStringValue:mLoadingStatusString];
    }      
  }
  else {
    [mStatus setStringValue:statusString];
  }
}

- (NSString *)title 
{
  return mTitle;
}

- (void)setTitle:(NSString *)title
{
  [mTitle autorelease];
  
  // We must be the primary content area to actually set the title, but we
  // still want to hold onto the title in case we become the primary later.
  NSString* newTitle = nil;
  if (mOffline) {
    if (title && ![title isEqualToString:@""])
        newTitle = [title stringByAppendingString: @" [Working Offline]"];	// XXX localize me
    else
        newTitle = [NSString stringWithString:@"Untitled [Working Offline]"];
    mTitle = [newTitle retain];
  }
  else {
    if (!title || [title isEqualToString:@""])
      title = [NSString stringWithString:NSLocalizedString(@"UntitledPageTitle", @"")];
    mTitle = [title retain];
  }

  if ( mIsPrimary && mWindowController )
    [[mWindowController window] setTitle:[mTitle stringByTruncatingTo:80 at:kTruncateAtEnd]];
  
  // Always set the tab.
  if (title && ![title isEqualToString:@""])
    [mTabItem setLabel:title];		// tab titles get truncated when setting them to tabs
  else
    [mTabItem setLabel:NSLocalizedString(@"UntitledPageTitle", @"")];
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

-(void)setIsBookmarksImport:(BOOL)aIsImport
{
  mIsBookmarksImport = aIsImport;
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
    PRBool isUnrequested;
    piWindow->IsLoadingOrRunningTimeout(&isUnrequested);
    if (isUnrequested) {
      // A popup is being opened while the page is currently loading.  Offer to block the
      // popup.
      nsAlertController* controller = nsCocoaBrowserService::GetAlertController();
      BOOL confirm = [controller confirm: [self window] title: @"Unrequested Popup Detected"
                                text: [NSString stringWithFormat: NSLocalizedString(@"PopupBlockMsg", @""), NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)]];

      // This is a one-time dialog.
      pref->SetBoolPref("browser.popups.showPopupBlocker", PR_FALSE);
      
      if (confirm) {
        pref->SetBoolPref("dom.disable_open_during_load", PR_TRUE);
        pref->SetIntPref("dom.disable_open_click_delay", 1000);
      }

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
  [controller enterModalSession];
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
  BOOL resetTabIcon = NO;
  
  if (![mSiteIconURI isEqualToString:inSiteIconURI])
  {
    if (!inSiteIcon)
    {
      if (![inSiteIconURI isEqualToString:@"about:blank"])
        inSiteIcon = [NSImage imageNamed:@"globe_ico"];
    }

    [self setSiteIconImage: inSiteIcon];
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
      [tabItem setTabIcon:mSiteIconImage];
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
    
    // NSLog(@"CHBrowserWrapper imageLoadedNotification got image %@ and uri %@", iconImage, proxyImageURI);
    if (iconImage == nil)
      siteIconURI = @"";	// go back to default image
    
    [self updateSiteIconImage:iconImage withURI:siteIconURI];
  }
}


@end
