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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef __nsCocoaBrowserView_h__
#define __nsCocoaBrowserView_h__

#undef DARWIN
#import <Cocoa/Cocoa.h>

#include "prtypes.h"
#include "nsCOMPtr.h"

@class CHBrowserView;
@class CertificateItem;

class CHBrowserListener;
class nsIDOMWindow;
class nsIWebBrowser;
class nsIDOMNode;
class nsIDOMEvent;
class nsIEventSink;
class nsIDragHelperService;
class nsIPrintSettings;
class nsIURI;
class nsISupports;
class nsISecureBrowserUI;

// Protocol implemented by anyone interested in progress
// related to a BrowserView. A listener should explicitly
// register itself with the view using the addListener
// method.
@protocol CHBrowserListener

- (void)onLoadingStarted;
- (void)onLoadingCompleted:(BOOL)succeeded;
// Invoked regularly as data associated with a page streams
// in. If the total number of bytes expected is unknown,
// maxBytes is -1.
- (void)onProgressChange:(long)currentBytes outOf:(long)maxBytes;
- (void)onProgressChange64:(long long)currentBytes outOf:(long long)maxBytes;

- (void)onLocationChange:(NSString*)urlSpec isNewPage:(BOOL)newPage requestSucceeded:(BOOL)requestOK;
- (void)onStatusChange:(NSString*)aMessage;
- (void)onSecurityStateChange:(unsigned long)newState;
// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode;
// Called when a tooltip should be shown or hidden
- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text;
- (void)onHideTooltip;
// Called when a popup is blocked
- (void)onPopupBlocked:(nsIURI*)inURIBlocked fromSite:(nsIURI*)inSite;
// Called when a "shortcut icon" link element is noticed
- (void)onFoundShortcutIcon:(NSString*)inIconURI;

@end

typedef enum {
  NSStatusTypeScript            = 0x0001,
  NSStatusTypeScriptDefault     = 0x0002,
  NSStatusTypeLink              = 0x0003,
} NSStatusType;

@protocol CHBrowserContainer

- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
// Set the dimensions of our NSView. The container might need to do
// some adjustment, so the view doesn't do it directly.
- (void)sizeBrowserTo:(NSSize)dimensions;

// Create a new browser container window and return the contained view. 
- (CHBrowserView*)createBrowserWindow:(unsigned int)mask;
// Return the view of the current window, or perhaps a new tab within that window,
// in which to load the request.
- (CHBrowserView*)reuseExistingBrowserWindow:(unsigned int)mask;

// Return whether the container prefers to create new windows or to re-use
// the existing one (will return YES if implementing "single-window mode")
- (BOOL)shouldReuseExistingWindow;

- (NSMenu*)getContextMenu;
- (NSWindow*)getNativeWindow;

// Ask whether the browser should accept a drag from the given source.
// Should return NO if the source is a container for the browser, or
// another item that represents the same entity (e.g. tab or proxy icon)
- (BOOL)shouldAcceptDragFromSource:(id)dragSource;

// Gecko wants to close the "window" associated with this instance. Some
// embedding apps might want to multiplex multiple gecko views in one
// window (tabbed browsing). This gives them the chance to change the
// behavior.
- (void)closeBrowserWindow;

// Called before and after a prompt is shown for the contained view
- (void)willShowPrompt;
- (void)didDismissPrompt;

@end

enum {
  NSLoadFlagsNone                   = 0x0000,
  NSLoadFlagsDontPutInHistory       = 0x0010,
  NSLoadFlagsReplaceHistoryEntry    = 0x0020,
  NSLoadFlagsBypassCacheAndProxy    = 0x0040
}; 

enum {
  NSStopLoadNetwork   = 0x01,
  NSStopLoadContent   = 0x02,
  NSStopLoadAll       = 0x03  
};

typedef enum {
  CHSecurityInsecure     = 0,
  CHSecuritySecure       = 1,
  CHSecurityBroken       = 2     // or mixed content
} CHSecurityStatus;

typedef enum {
  CHSecurityNone          = 0,
  CHSecurityLow           = 1,
  CHSecurityMedium        = 2,     // key strength < 90 bit
  CHSecurityHigh          = 3
} CHSecurityStrength;

@interface CHBrowserView : NSView 
{
  nsIWebBrowser*        _webBrowser;
  CHBrowserListener*    _listener;
  NSWindow*             mWindow;
  
  nsIDragHelperService* mDragHelper;
  NSPoint               mLastTrackedLocation;
  NSWindow*             mLastTrackedWindow;
  
  nsIPrintSettings*     mPrintSettings; // we own this
  BOOL                  mUseGlobalPrintSettings;
}

// class method to get at the browser view for a given nsIDOMWindow
+ (CHBrowserView*)browserViewFromDOMWindow:(nsIDOMWindow*)inWindow;

// NSView overrides
- (id)initWithFrame:(NSRect)frame;
- (id)initWithFrame:(NSRect)frame andWindow:(NSWindow*)aWindow;

- (void)dealloc;
- (void)setFrame:(NSRect)frameRect;

// nsIWebBrowser methods
- (void)addListener:(id <CHBrowserListener>)listener;
- (void)removeListener:(id <CHBrowserListener>)listener;
- (void)setContainer:(NSView<CHBrowserListener, CHBrowserContainer>*)container;
- (already_AddRefed<nsIDOMWindow>)getContentWindow;	// addrefs

// nsIWebNavigation methods
- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags allowPopups:(BOOL)inAllowPopups;
- (void)reload:(unsigned int)flags;
- (void)goBack;
- (BOOL)canGoBack;
- (void)goForward;
- (BOOL)canGoForward;
- (void)stop:(unsigned int)flags;   // NSStop flags
- (void)goToSessionHistoryIndex:(int)index;

- (NSString*)getCurrentURI;

- (NSString*)pageLocation;  // from window.location. can differ from the document's URI, and possibly from getCurrentURI
- (NSString*)pageLocationHost;
- (NSString*)pageTitle;
- (NSDate*)pageLastModifiedDate;

// nsIWebBrowserSetup methods
- (void)setProperty:(unsigned int)property toValue:(unsigned int)value;

- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView;
- (void)saveURL:(NSView*)aFilterView url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename;

- (void)printDocument;
- (void)pageSetup;

- (BOOL)findInPageWithPattern:(NSString*)inText caseSensitive:(BOOL)inCaseSensitive
            wrap:(BOOL)inWrap backwards:(BOOL)inBackwards;

- (BOOL)findInPage:(BOOL)inBackwards;
- (NSString*)lastFindText;

-(BOOL)validateMenuItem: (NSMenuItem*)aMenuItem;

-(IBAction)cut:(id)aSender;
-(BOOL)canCut;
-(IBAction)copy:(id)aSender;
-(BOOL)canCopy;
-(IBAction)paste:(id)aSender;
-(BOOL)canPaste;
-(IBAction)delete:(id)aSender;
-(BOOL)canDelete;
-(IBAction)selectAll:(id)aSender;

// Returns the currently selected text as a NSString. 
- (NSString*)getSelection;

-(IBAction)undo:(id)aSender;
-(IBAction)redo:(id)aSender;

- (BOOL)canUndo;
- (BOOL)canRedo;

- (void)biggerTextSize;
- (void)smallerTextSize;

- (BOOL)canMakeTextBigger;
- (BOOL)canMakeTextSmaller;

// ideally these would not have to be called from outside the CHBrowerView, but currently
// the cocoa impl of nsIPromptService is at the app level, so it needs to call down
// here. We'll just turn around and call the CHBrowserContainer methods
- (void)doBeforePromptDisplay;
- (void)doAfterPromptDismissal;

- (void)setActive: (BOOL)aIsActive;

- (NSMenu*)getContextMenu;
- (NSWindow*)getNativeWindow;

- (void)destroyWebBrowser;
- (nsIWebBrowser*)getWebBrowser;
- (CHBrowserListener*)getCocoaBrowserListener;
- (void)setWebBrowser:(nsIWebBrowser*)browser;

- (NSString*)getFocusedURLString;

// charset
- (IBAction)reloadWithNewCharset:(NSString*)charset;
- (NSString*)currentCharset;

// security
- (BOOL)hasSSLStatus;   // if NO, then the following methods all return empty values.
- (unsigned int)secretKeyLength;
- (NSString*)cipherName;
- (CHSecurityStatus)securityStatus;
- (CHSecurityStrength)securityStrength;
- (CertificateItem*)siteCertificate;

- (already_AddRefed<nsISupports>)getPageDescriptor;
- (void)setPageDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType;

@end

#endif // __nsCocoaBrowserView_h__
