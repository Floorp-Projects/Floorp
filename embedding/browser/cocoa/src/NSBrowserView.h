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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsCocoaBrowserView_h__
#define __nsCocoaBrowserView_h__

#import <Cocoa/Cocoa.h>

@class NSBrowserView;
class nsCocoaBrowserListener;
class nsIDOMWindow;
class nsIWebBrowser;

// Protocol implemented by anyone interested in progress
// related to a BrowserView. A listener should explicitly
// register itself with the view using the addListener
// method.
@protocol NSBrowserListener
- (void)onLoadingStarted;
- (void)onLoadingCompleted:(BOOL)succeeded;
// Invoked regularly as data associated with a page streams
// in. If the total number of bytes expected is unknown,
// maxBytes is -1.
- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes;
- (void)onLocationChange:(NSURL*)url;
@end

typedef enum {
  NSStatusTypeScript            = 0x0001,
  NSStatusTypeScriptDefault     = 0x0002,
  NSStatusTypeLink              = 0x0003,
} NSStatusType;

@protocol NSBrowserContainer
- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
// Set the dimensions of our NSView. The container might need to do
// some adjustment, so the view doesn't do it directly.
- (void)sizeBrowserTo:(NSSize)dimensions;
// Create a new browser container window and return the contained view. 
- (NSBrowserView*)createBrowserWindow:(unsigned int)mask;
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

@interface NSBrowserView : NSView 
{
  nsIWebBrowser* _webBrowser;
  nsCocoaBrowserListener* _listener;
}

// NSView overrides
- (id)initWithFrame:(NSRect)frame;
- (void)dealloc;
- (void)setFrame:(NSRect)frameRect;

// nsIWebBrowser methods
- (void)addListener:(id <NSBrowserListener>)listener;
- (void)removeListener:(id <NSBrowserListener>)listener;
- (void)setContainer:(id <NSBrowserContainer>)container;
- (nsIDOMWindow*)getContentWindow;

// nsIWebNavigation methods
- (void)loadURI:(NSURL *)url flags:(unsigned int)flags;
- (void)reload:(unsigned int)flags;
- (BOOL)canGoBack;
- (BOOL)canGoForward;
- (void)goBack;
- (void)goForward;
- (void)gotoIndex:(int)index;
- (void)stop:(unsigned int)flags;
- (NSURL*)getCurrentURI;

- (nsIWebBrowser*)getWebBrowser;
- (void)setWebBrowser:(nsIWebBrowser*)browser;
@end

#endif // __nsCocoaBrowserView_h__
