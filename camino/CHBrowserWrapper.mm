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

#import "CHBrowserWrapper.h"
#import "BrowserWindowController.h"
#import "BookmarksService.h"

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

#define DOCUMENT_DONE_STRING @"Document: Done"
#define LOADING_STRING @"Loading..."

static const char* ioServiceContractID = "@mozilla.org/network/io-service;1";

@implementation CHBrowserWrapper

-(void)windowClosed
{
  // Break the cycle.
  [mBrowserView destroyWebBrowser];
  [mBrowserView setContainer: nil];
  [mBrowserView removeListener: self];
}

-(void)dealloc
{
  printf("The browser wrapper died.\n");

  [[NSNotificationCenter defaultCenter] removeObserver: self];
    
	[defaultStatus release];
  [loadingStatus release];

  [super dealloc];
}

- (IBAction)load:(id)sender
{
  [mBrowserView loadURI:[NSURL URLWithString:[urlbar stringValue]] flags:NSLoadFlagsNone];
}

-(void)disconnectView
{
  urlbar = nil;
  status = nil;
  progress = nil;
  progressSuper = nil;
  mIsPrimary = NO;
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mBrowserView setActive: NO];
}

-(id)initWithTab:(id)aTab andWindow: (NSWindow*)aWindow
{
  mTab = aTab;
  mWindow = aWindow;
  mIsBookmarksImport = NO;
  return [self initWithFrame: NSZeroRect];
}

- (id)initWithFrame:(NSRect)frameRect
{
  if ( (self = [super initWithFrame: frameRect]) ) {
    mBrowserView = [[[CHBrowserView alloc] initWithFrame:[self bounds] andWindow: [self getNativeWindow]] autorelease];
    [self addSubview: mBrowserView];
    [mBrowserView setContainer:self];
    [mBrowserView addListener:self];
    mIsBusy = NO;
    mListenersAttached = NO;
  }
  return self;
}

-(void)setTab: (NSTabViewItem*)tab
{
  mTab = tab;
}

-(void)makePrimaryBrowserView: (id)aUrlbar status: (id)aStatus
    progress: (id)aProgress windowController: aWindowController
{
  urlbar = aUrlbar;
  status = aStatus;
  progress = aProgress;
  progressSuper = [aProgress superview];
  mWindowController = aWindowController;
  
  if (!mIsBusy)
    [progress removeFromSuperview];
  
  defaultStatus = NULL;
  loadingStatus = DOCUMENT_DONE_STRING;
  [status setStringValue:loadingStatus];
  
  mIsPrimary = YES;

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
        name:@"offlineModeChanged"
        object:nil];
        
  // Update the URL bar.
  [mWindowController updateLocationFields:[self getCurrentURLSpec]];
  
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

- (void)onLoadingStarted 
{
  if (defaultStatus) {
    [defaultStatus release];
    defaultStatus = NULL;
  }

  [progressSuper addSubview:progress];
  [progress setIndeterminate:YES];
  [progress startAnimation:self];

  loadingStatus = LOADING_STRING;
  [status setStringValue:loadingStatus];

  mIsBusy = YES;
  [mTab setLabel: LOADING_STRING];
  
  if (mWindowController)
    [mWindowController updateToolbarItems];
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  [progress setIndeterminate:YES];
  [progress stopAnimation:self];
  [progress removeFromSuperview];

  loadingStatus = DOCUMENT_DONE_STRING;
  if (defaultStatus) {
    [status setStringValue:defaultStatus];
  }
  else {
    [status setStringValue:loadingStatus];
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

  if (mWindowController)
    [mWindowController updateToolbarItems];
}

- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes 
{
  if (maxBytes > 0) {
    BOOL isIndeterminate = [progress isIndeterminate];
    if (isIndeterminate) {
      [progress setIndeterminate:NO];
    }
    double val = ((double)currentBytes / (double)maxBytes) * 100.0;
    [progress setDoubleValue:val];
  }
}

- (void)onLocationChange:(NSURL*)url 
{
  NSString* spec = [url absoluteString];
  [mWindowController updateLocationFields:spec];
}

- (void)onStatusChange:(NSString*)aStatusString
{
  [status setStringValue: aStatusString];
}

- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type 
{
  if (type == NSStatusTypeScriptDefault) {
    if (defaultStatus) {
      [defaultStatus release];
    }
    defaultStatus = statusString;
    if (defaultStatus) {
      [defaultStatus retain];
    }
  }
  else if (!statusString) {
    if (defaultStatus) {
      [status setStringValue:defaultStatus];
    }
    else {
      [status setStringValue:loadingStatus];
    }      
  }
  else {
    [status setStringValue:statusString];
  }
}

- (NSString *)title 
{
  return [[mWindowController window] title];
}

- (void)setTitle:(NSString *)title
{
    // We must be the primary content area.
    if (mIsPrimary && mWindowController) {
        if (mOffline) {
            NSString* newTitle;
            if (title && ![title isEqualToString:@""])
                newTitle = [title stringByAppendingString: @" [Working Offline]"];
            else
                newTitle = @"Untitled [Working Offline]";
            [[mWindowController window] setTitle: newTitle];
        }
        else {
            if (title && ![title isEqualToString:@""])
                [[mWindowController window] setTitle:title];
            else
                [[mWindowController window] setTitle:@"Untitled"];
        }
    }
    
    // Always set the tab.
    if (title && ![title isEqualToString:@""])
        [mTab setLabel:title];
    else
        [mTab setLabel:@"Untitled"];
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

- (void)sizeBrowserTo:(NSSize)dimensions
{
  NSRect bounds = [self bounds];
  float dx = dimensions.width - bounds.size.width;
  float dy = dimensions.height - bounds.size.height;

  NSRect frame = [[self window] frame];
  frame.size.width += dx;
  frame.size.height += dy;

  [[self window] setFrame:frame display:YES];
}

#define NS_POPUP_BLOCK @"This Web site is attempting to open an unrequested popup window.  Navigator can \
automatically prevent Web sites from opening popup advertisements.  Click OK to prevent all \
unrequested popups (including this one) from opening."

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
                                text: NS_POPUP_BLOCK];

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
  [controller enterModalSession];
  [[[controller getBrowserWrapper] getBrowserView] setActive: YES];
  return [[controller getBrowserWrapper] getBrowserView];
}

- (CHBrowserView*)getBrowserView
{
  return mBrowserView;
}

@end
