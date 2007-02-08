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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
 *   Nick Kreeger <nick.kreeger@park.edu>
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

#import <Cocoa/Cocoa.h>

#import "NSString+Utils.h"

#import "mozView.h"

// Embedding includes
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMBarProp.h"

// XPCOM and String includes
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRequest.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsNetError.h"
#include "nsNetUtil.h"

#import "CHBrowserView.h"

#import "CHBrowserListener.h"


// informal protocol of methods that our embedding window might support
@interface NSWindow(BrowserWindow)

- (BOOL)suppressMakeKeyFront;

@end

CHBrowserListener::CHBrowserListener(CHBrowserView* aView)
  : mView(aView), mContainer(nsnull), mIsModal(PR_FALSE), mChromeFlags(0)
{
  mListeners = [[NSMutableArray alloc] init];
}

CHBrowserListener::~CHBrowserListener()
{
  [mListeners release];
  mView = nsnull;
  [mContainer release];
}

// Gecko's macros only go to 11, but this baby goes to 12!
#define NS_IMPL_QUERY_INTERFACE12(_class, _i1, _i2, _i3, _i4, _i5, _i6,       \
                                  _i7, _i8, _i9, _i10, _i11, _i12)            \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                              \
    NS_INTERFACE_MAP_ENTRY(_i11)                                              \
    NS_INTERFACE_MAP_ENTRY(_i12)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
  NS_INTERFACE_MAP_END
#define NS_IMPL_ISUPPORTS12(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10, _i11, _i12)                            \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE12(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10, _i11, _i12)

NS_IMPL_ISUPPORTS12(CHBrowserListener,
                   nsIInterfaceRequestor,
                   nsIWebBrowserChrome,
                   nsIWindowCreator,
                   nsIWindowProvider,
                   nsIEmbeddingSiteWindow,
                   nsIEmbeddingSiteWindow2,
                   nsIWebProgressListener,
                   nsIWebProgressListener2,
                   nsISupportsWeakReference,
                   nsIContextMenuListener,
                   nsIDOMEventListener,
                   nsITooltipListener)

// Implementation of nsIInterfaceRequestor
NS_IMETHODIMP 
CHBrowserListener::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    nsCOMPtr<nsIWebBrowser> browser = dont_AddRef([mView getWebBrowser]);
    if (browser)
      return browser->GetContentDOMWindow((nsIDOMWindow **) aInstancePtr);
  }
  
  return QueryInterface(aIID, aInstancePtr);
}

// Implementation of nsIWindowCreator.  The CocoaBrowserService forwards requests
// for a new window that have a parent to us, and we take over from there.  
/* nsIWebBrowserChrome createChromeWindow (in nsIWebBrowserChrome parent, in PRUint32 chromeFlags); */
NS_IMETHODIMP 
CHBrowserListener::CreateChromeWindow(nsIWebBrowserChrome *parent, 
                                           PRUint32 chromeFlags, 
                                           nsIWebBrowserChrome **_retval)
{
  if (parent != this) {
#if DEBUG
    NSLog(@"Mismatch in CHBrowserListener::CreateChromeWindow.  We should be the owning parent.");
#endif
    return NS_ERROR_FAILURE;
  }
  
  CHBrowserView* childView = [mContainer createBrowserWindow: chromeFlags];
  if (!childView) {
#if DEBUG
    NSLog(@"No CHBrowserView hooked up for a newly created window yet.");
#endif
    return NS_ERROR_FAILURE;
  }
  
  CHBrowserListener* listener = [childView getCocoaBrowserListener];
  if (!listener) {
#if DEBUG
    NSLog(@"Uh-oh! No listener yet for a newly created window (nsCocoaBrowserlistener)");
    return NS_ERROR_FAILURE;
#endif
  }
  
  // set the chrome flags on the listener of the newly created window. It 
  // uses these to know when it should open new windows when asked to provide
  // or re-use one
  listener->SetChromeFlags(chromeFlags);
  
#if DEBUG
  NSLog(@"Made a chrome window.");
#endif
  
  // apply scrollbar chrome flags
  if (!(chromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS))
  {
    nsCOMPtr<nsIDOMWindow> contentWindow = [childView getContentWindow];
    if (contentWindow)
    {
      nsCOMPtr<nsIDOMBarProp> scrollbars;
      contentWindow->GetScrollbars(getter_AddRefs(scrollbars));
      if (scrollbars)
        scrollbars->SetVisible(PR_FALSE);
    }
  }

  *_retval = listener;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

//
// ProvideWindow
//
// Called when Gecko wants to open a new window. We check our prefs and if they're
// set to reuse the existing window, we ask the container for a dom window (could be an 
// existing one or from a newly created tab) and tell Gecko to use that. Setting
// |outDOMWindow| to NULL tells Gecko to create a new window.
//
NS_IMETHODIMP
CHBrowserListener::ProvideWindow(nsIDOMWindow *inParent, PRUint32 inChromeFlags, PRBool aPositionSpecified, PRBool 
                                  aSizeSpecified, nsIURI *aURI, const nsAString & aName, const nsACString & aFeatures,
                                  PRBool *outWindowIsNew, nsIDOMWindow **outDOMWindow)
{
  NS_ENSURE_ARG_POINTER(outDOMWindow);
  *outDOMWindow = NULL;

  // if the window has any non-standard chrome or a specific size/position and our pref says to respect such calls, let
  // Gecko open a new window.
  if ((inChromeFlags != nsIWebBrowserChrome::CHROME_ALL || aPositionSpecified || aSizeSpecified) &&
      [mContainer respectWindowOpenCallsWithSizeAndPosition])
    return NS_OK;

  // if our chrome flags are different than normal, also force creating a new window
  if (mChromeFlags && mChromeFlags != nsIWebBrowserChrome::CHROME_ALL)
    return NS_OK;

  // if the container prefers to reuse the existing window, tell it to do so and return
  // the DOMWindow it gives us. Otherwise we'll let Gecko create a new window.
  BOOL prefersTabs = [mContainer shouldReuseExistingWindow];
  if (prefersTabs) {
    CHBrowserView* newContainer = [mContainer reuseExistingBrowserWindow:inChromeFlags];
    nsCOMPtr<nsIDOMWindow> contentWindow = [newContainer getContentWindow];
    
    // make sure gecko knows whether we're creating a new browser window (new tabs don't count)
    nsCOMPtr<nsIDOMWindow> currentWindow = [mView getContentWindow];
    *outWindowIsNew = (contentWindow != currentWindow);
    
    NS_IF_ADDREF(*outDOMWindow = contentWindow.get());
  }

  return NS_OK;
}

// Implementation of nsIContextMenuListener
NS_IMETHODIMP
CHBrowserListener::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent* aEvent, nsIDOMNode* aNode)
{
  [mContainer onShowContextMenu: aContextFlags domEvent: aEvent domNode: aNode];
  return NS_OK;
}

// Implementation of nsITooltipListener
NS_IMETHODIMP
CHBrowserListener::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
  NSPoint where;
  where.x = aXCoords; where.y = aYCoords;
  [mContainer onShowTooltip:where withText:[NSString stringWithPRUnichars:aTipText]];
  return NS_OK;
}

NS_IMETHODIMP
CHBrowserListener::OnHideTooltip()
{
  [mContainer onHideTooltip];
  return NS_OK;
}

// Implementation of nsIWebBrowserChrome
/* void setStatus (in unsigned long statusType, in wstring status); */
NS_IMETHODIMP 
CHBrowserListener::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* str = nsnull;
  if (status && (*status != PRUnichar(0))) {
    str = [NSString stringWithPRUnichars:status];
  }

  [mContainer setStatus:str ofType:(NSStatusType)statusType];

  return NS_OK;
}

/* attribute nsIWebBrowser webBrowser; */
NS_IMETHODIMP 
CHBrowserListener::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  NS_ENSURE_ARG_POINTER(aWebBrowser);
  if (!mView) {
    return NS_ERROR_FAILURE;
  }
  *aWebBrowser = [mView getWebBrowser];

  return NS_OK;
}
NS_IMETHODIMP 
CHBrowserListener::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  [mView setWebBrowser:aWebBrowser];

  return NS_OK;
}

/* attribute unsigned long chromeFlags; */
NS_IMETHODIMP 
CHBrowserListener::GetChromeFlags(PRUint32 *aChromeFlags)
{
  NS_ENSURE_ARG_POINTER(aChromeFlags);
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP 
CHBrowserListener::SetChromeFlags(PRUint32 aChromeFlags)
{
  mChromeFlags = aChromeFlags;
  return NS_OK;
}

/* void destroyBrowserWindow (); */
NS_IMETHODIMP 
CHBrowserListener::DestroyBrowserWindow()
{
  // tell the container we want to close the window and let it do the
  // right thing.
  [mContainer closeBrowserWindow];
  return NS_OK;
}

/* void sizeBrowserTo (in long aCX, in long aCY); */
NS_IMETHODIMP 
CHBrowserListener::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  if (mContainer) {
    NSSize size;
    
    size.width = (float)aCX;
    size.height = (float)aCY;

    [mContainer sizeBrowserTo:size];
  }
  
  return NS_OK;
}

/* void showAsModal (); */
NS_IMETHODIMP 
CHBrowserListener::ShowAsModal()
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView nativeWindow];

  if (!window) {
    return NS_ERROR_FAILURE;
  }

  mIsModal = PR_TRUE;
  //int result = [NSApp runModalForWindow:window];
  mIsModal = PR_FALSE;

  return NS_OK;
}

/* boolean isWindowModal (); */
NS_IMETHODIMP 
CHBrowserListener::IsWindowModal(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mIsModal;

  return NS_OK;
}

/* void exitModalEventLoop (in nsresult aStatus); */
NS_IMETHODIMP 
CHBrowserListener::ExitModalEventLoop(nsresult aStatus)
{
//  [NSApp stopModalWithCode:(int)aStatus];

  return NS_OK;
}

// Implementation of nsIEmbeddingSiteWindow2
NS_IMETHODIMP
CHBrowserListener::Blur()
{
  return NS_OK;
}

// Implementation of nsIEmbeddingSiteWindow
/* void setDimensions (in unsigned long flags, in long x, in long y, in long cx, in long cy); */
NS_IMETHODIMP 
CHBrowserListener::SetDimensions(PRUint32 flags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
  if (!mView)
    return NS_ERROR_FAILURE;

  // use -window here and not -nativeWindow because we don't want to allow bg tabs
  // (which aren't in the window hierarchy) to resize the window.
  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
  {
    NSPoint origin;
    origin.x = (float)x;
    origin.y = (float)y;
    
    // websites assume the origin is the topleft of the window and that the screen origin
    // is "topleft" (quickdraw coordinates). As a result, we have to convert it.
    GDHandle screenDevice = ::GetMainDevice();
    Rect screenRect = (**screenDevice).gdRect;
    short screenHeight = screenRect.bottom - screenRect.top;
    origin.y = screenHeight - origin.y;
    
    [window setFrameTopLeftPoint:origin];
  }

  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
  {
    NSRect frame = [window frame];
    
    // should we allow resizes larger than the screen, or smaller
    // than some min size here?
    
    // keep the top of the window in the same place
    frame.origin.y += (frame.size.height - (float)cy);
    frame.size.width = (float)cx;
    frame.size.height = (float)cy;
    [window setFrame:frame display:YES];
  }
  else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
  {
    NSSize size;
    size.width = (float)cx;
    size.height = (float)cy;
    [window setContentSize:size];
  }

  return NS_OK;
}

/* void getDimensions (in unsigned long flags, out long x, out long y, out long cx, out long cy); */
NS_IMETHODIMP 
CHBrowserListener::GetDimensions(PRUint32 flags,  PRInt32 *x,  PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
  if (!mView)
    return NS_ERROR_FAILURE;

  NSWindow* window = [mView nativeWindow];
  if (!window)
    return NS_ERROR_FAILURE;

  NSRect frame = [window frame];
  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    if ( x )
      *x = (PRInt32)frame.origin.x;
    if ( y ) {
      // websites (and gecko) expect the |y| value to be in "quickdraw" coordinates 
      // (topleft of window, origin is topleft of main device). Convert from cocoa -> 
      // quickdraw coord system.
      GDHandle screenDevice = ::GetMainDevice();
      Rect screenRect = (**screenDevice).gdRect;
      short screenHeight = screenRect.bottom - screenRect.top;
      *y = screenHeight - (PRInt32)(frame.origin.y + frame.size.height);
    }
  }
  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    if ( cx )
      *cx = (PRInt32)frame.size.width;
    if ( cy )
      *cy = (PRInt32)frame.size.height;
  }
  else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER) {
    NSView* contentView = [window contentView];
    NSRect contentFrame = [contentView frame];
    if ( cx )
      *cx = (PRInt32)contentFrame.size.width;
    if ( cy )
      *cy = (PRInt32)contentFrame.size.height;    
  }

  return NS_OK;
}

/* void setFocus (); */
NS_IMETHODIMP 
CHBrowserListener::SetFocus()
{
  // don't use -nativeWindow here so tabs in the bg can't take focus
  NSWindow* window = [mView window];
  if (!window) 
    return NS_ERROR_FAILURE;
  
  // if we're already the keyWindow, we certainly don't need to do it again. This
  // ends up fixing a problem where we try to bring ourselves to the front while we're
  // in the process of miniaturizing or showing the window
  if ([window isVisible] && (window != [NSApp keyWindow]))
  {
    BOOL suppressed = NO;
    if ([window respondsToSelector:@selector(suppressMakeKeyFront)])
      suppressed = [window suppressMakeKeyFront];
  
    if (!suppressed)
      [window makeKeyAndOrderFront:window];
  }

  return NS_OK;
}

/* attribute boolean visibility; */
NS_IMETHODIMP 
CHBrowserListener::GetVisibility(PRBool *aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);
  *aVisibility = PR_FALSE;
  
  if (!mView)
    return NS_ERROR_FAILURE;

  // Only return PR_TRUE if the view is the current tab
  // (so its -window is non-nil). See bug 306245.
  // XXX should we bother testing [window isVisible]?
  *aVisibility = [mView window] && [[mView window] isVisible];
  return NS_OK;
}

NS_IMETHODIMP 
CHBrowserListener::SetVisibility(PRBool aVisibility)
{
  // use -window instead of -nativeWindow to prevent bg tabs from being able to
  // change the visibility
  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

  // we rely on this callback to show gecko-created windows
  if (aVisibility)	// showing
  {
    BOOL suppressed = NO;
    if ([window respondsToSelector:@selector(suppressMakeKeyFront)])
      suppressed = [window suppressMakeKeyFront];
    
    if (![window isVisible] && !suppressed)
      [window makeKeyAndOrderFront:nil];
  }
  else						// hiding
  {
    // XXX should we really hide a window that may have other tabs?
    if ([window isVisible])
      [window orderOut:nil];
  }
  
  return NS_OK;
}

/* attribute wstring title; */
NS_IMETHODIMP 
CHBrowserListener::GetTitle(PRUnichar * *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);

  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* title = [mContainer title];
  if ([title length] > 0)
    *aTitle = [title createNewUnicodeBuffer];
  else
    *aTitle = nsnull;
  
  return NS_OK;
}
NS_IMETHODIMP 
CHBrowserListener::SetTitle(const PRUnichar * aTitle)
{
  NS_ENSURE_ARG(aTitle);

  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* str = [NSString stringWithPRUnichars:aTitle];
  [mContainer setTitle:str];

  return NS_OK;
}

/* [noscript] readonly attribute voidPtr siteWindow; */
// We return the CHBrowserView here, which isn't a window, but allows callers
// to tell which tab something is coming from.
NS_IMETHODIMP 
CHBrowserListener::GetSiteWindow(void * *aSiteWindow)
{
  NS_ENSURE_ARG_POINTER(aSiteWindow);
  *aSiteWindow = nsnull;
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  if (!mView)
    return NS_ERROR_FAILURE;

  *aSiteWindow = (void*)mView;

  return NS_OK;
}

//
// Implementation of nsIWebProgressListener2
//

/* void onProgressChange64 (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long long aCurSelfProgress, in long long aMaxSelfProgress, in long long aCurTotalProgress, in long long aMaxTotalProgress); */
NS_IMETHODIMP 
CHBrowserListener::OnProgressChange64(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                       PRInt64 aCurSelfProgress, PRInt64 aMaxSelfProgress, 
                                       PRInt64 aCurTotalProgress, PRInt64 aMaxTotalProgress)
{
  //XXXPINK there appear to be a compiler bug here, the values passed to |-onProgressChange64:outOf:|
  // are garbage even though they're ok here.
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj;
  while ((obj = [enumerator nextObject]))
    [obj onProgressChange64:aCurTotalProgress outOf:aMaxTotalProgress];
  
  return NS_OK;
}

/* boolean onRefreshAttempted (in nsIWebProgress aWebProgress, in nsIURI aRefreshURI, in long aDelay, in boolean aSameURI); */
NS_IMETHODIMP
CHBrowserListener::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                      nsIURI *aUri,
                                      PRInt32 aDelay,
                                      PRBool aSameUri,
                                      PRBool *allowRefresh)
{
    *allowRefresh = PR_TRUE;
    return NS_OK;
}

//
// Implementation of nsIWebProgressListener
//

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
CHBrowserListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                          PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
                                          PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj;
  while ((obj = [enumerator nextObject]))
    [obj onProgressChange:aCurTotalProgress outOf:aMaxTotalProgress];
  
  return NS_OK;
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
CHBrowserListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                        PRUint32 aStateFlags, PRUint32 aStatus)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj;
  if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)
  {
    if (aStateFlags & nsIWebProgressListener::STATE_START)
    {
      while ((obj = [enumerator nextObject]))
        [obj onLoadingStarted];
    }
    else if (aStateFlags & nsIWebProgressListener::STATE_STOP)
    {
      while ((obj = [enumerator nextObject]))
        [obj onLoadingCompleted:(NS_SUCCEEDED(aStatus))];
    }
  }

  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
CHBrowserListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                          nsIURI *aLocation)
{
  if (!aLocation || !aWebProgress)
    return NS_ERROR_FAILURE;

  // only pay attention to location change for our nsIDOMWindow
  nsCOMPtr<nsIDOMWindow> windowForProgress;
  aWebProgress->GetDOMWindow(getter_AddRefs(windowForProgress));
  nsCOMPtr<nsIDOMWindow> ourWindow = do_GetInterface(NS_STATIC_CAST(nsIInterfaceRequestor*, this));
  if (windowForProgress != ourWindow)
    return NS_OK;

  BOOL requestOK = YES;
  if (aRequest)  // aRequest can be null (e.g. for relative anchors)
  {
    nsresult requestStatus = NS_OK;
    aRequest->GetStatus(&requestStatus);
    requestOK = NS_SUCCEEDED(requestStatus);
  }
  
  nsCAutoString spec;
  nsCOMPtr<nsIURI> exposableLocation;
  nsCOMPtr<nsIURIFixup> fixup(do_GetService("@mozilla.org/docshell/urifixup;1"));
  if (fixup && NS_SUCCEEDED(fixup->CreateExposableURI(aLocation, getter_AddRefs(exposableLocation))) && exposableLocation)
    exposableLocation->GetSpec(spec);
  else
    aLocation->GetSpec(spec);

  NSString* str = [NSString stringWithUTF8String:spec.get()];

  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj;
  while ((obj = [enumerator nextObject]))
    [obj onLocationChange:str isNewPage:(aRequest != nsnull) requestSucceeded:requestOK];

  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
CHBrowserListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, 
                                        const PRUnichar *aMessage)
{
  NSString* str = [NSString stringWithPRUnichars:aMessage];
  
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj; 
  while ((obj = [enumerator nextObject]))
    [obj onStatusChange: str];

  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
CHBrowserListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<CHBrowserListener> obj; 
  while ((obj = [enumerator nextObject]))
    [obj onSecurityStateChange: state];

  return NS_OK;
}

void 
CHBrowserListener::AddListener(id <CHBrowserListener> aListener)
{
  [mListeners addObject:aListener];
}

void 
CHBrowserListener::RemoveListener(id <CHBrowserListener> aListener)
{
  [mListeners removeObject:aListener];
}

void 
CHBrowserListener::SetContainer(NSView<CHBrowserListener, CHBrowserContainer>* aContainer)
{
  [mContainer autorelease];
  mContainer = aContainer;
  [mContainer retain];
}

NS_IMETHODIMP
CHBrowserListener::HandleEvent(nsIDOMEvent* inEvent)
{
  NS_ENSURE_ARG(inEvent);
  
  nsAutoString eventType;
  inEvent->GetType(eventType);
  
  if (eventType.Equals(NS_LITERAL_STRING("DOMPopupBlocked")))
    return HandleBlockedPopupEvent(inEvent);

  if (eventType.Equals(NS_LITERAL_STRING("DOMLinkAdded")))
    return HandleLinkAddedEvent(inEvent);

  return NS_OK;
}


nsresult
CHBrowserListener::HandleBlockedPopupEvent(nsIDOMEvent* inEvent)
{
  nsCOMPtr<nsIDOMPopupBlockedEvent> blockedPopupEvent = do_QueryInterface(inEvent);
  if (blockedPopupEvent)
    [mContainer onPopupBlocked:blockedPopupEvent];

  return NS_OK;
}

nsresult
CHBrowserListener::HandleLinkAddedEvent(nsIDOMEvent* inEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  inEvent->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMElement> linkElement = do_QueryInterface(target);
  if (!linkElement)
    return NS_ERROR_FAILURE;
  
  ELinkAttributeType linkAttrType = GetLinkAttributeType(linkElement);
  if (linkAttrType == eOtherType)
    return NS_OK;
  
  nsAutoString relAttribute;
  linkElement->GetAttribute(NS_LITERAL_STRING("rel"), relAttribute);
  
  if (linkAttrType == eFavIconType)
    HandleFaviconLink(linkElement);
  else if (linkAttrType == eFeedType)
    HandleFeedLink(linkElement);

  return NS_OK;
}

ELinkAttributeType
CHBrowserListener::GetLinkAttributeType(nsIDOMElement* inElement)
{
  nsAutoString attribute;
  inElement->GetAttribute(NS_LITERAL_STRING("rel"), attribute);
  
  // Favicon link type
  if (attribute.EqualsIgnoreCase("shortcut icon") || attribute.EqualsIgnoreCase("icon"))
    return eFavIconType;

  // Check for feed type next
  inElement->GetAttribute(NS_LITERAL_STRING("type"), attribute);
  
  // kinda ugly, but if we don't match any of these strings return
  if (attribute.Equals(NS_LITERAL_STRING("application/rssxml")) ||
      attribute.Equals(NS_LITERAL_STRING("application/rss+xml")) ||
      attribute.Equals(NS_LITERAL_STRING("application/atomxml")) ||
      attribute.Equals(NS_LITERAL_STRING("application/atom+xml")) ||
      attribute.Equals(NS_LITERAL_STRING("text/xml")) ||
      attribute.Equals(NS_LITERAL_STRING("application/xml")) ||
      attribute.Equals(NS_LITERAL_STRING("application/rdfxml")))
  {
    return eFeedType;
  }
  
  return eOtherType;
}

void
CHBrowserListener::HandleFaviconLink(nsIDOMElement* inElement)
{  
  // make sure the load is for the main window
  nsCOMPtr<nsIDOMDocument> domDoc;
  inElement->GetOwnerDocument(getter_AddRefs(domDoc));
  
  nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(domDoc));
  if (!docView)
    return;
  
  nsCOMPtr<nsIDOMAbstractView> abstractView;
  docView->GetDefaultView(getter_AddRefs(abstractView));
  if (!abstractView)
    return;
  
  nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(abstractView));;
  if (!domWin)
    return;
  
  nsCOMPtr<nsIDOMWindow> topDomWin;
  domWin->GetTop(getter_AddRefs(topDomWin));
  
  nsCOMPtr<nsISupports> domWinAsISupports(do_QueryInterface(domWin));
  nsCOMPtr<nsISupports> topDomWinAsISupports(do_QueryInterface(topDomWin));
  // prevent subframes from setting the favicon
  if (domWinAsISupports != topDomWinAsISupports)
    return;
  
  // get the uri of the icon
  nsAutoString iconHref;
  inElement->GetAttribute(NS_LITERAL_STRING("href"), iconHref);
  if (iconHref.IsEmpty())
    return;
  
  // get the document uri
  nsCOMPtr<nsIDOM3Document> doc = do_QueryInterface(domDoc);
  if (!doc)
    return;
  
  nsAutoString docURISpec;
  nsresult rv = doc->GetDocumentURI(docURISpec);
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIURI> documentURI;
  rv = NS_NewURI(getter_AddRefs(documentURI), docURISpec);
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIURI> iconURI;
  rv = NS_NewURI(getter_AddRefs(iconURI), NS_ConvertUTF16toUTF8(iconHref), nsnull, documentURI);
  if (NS_FAILED(rv))
    return;
  
  // only accept http and https icons (should we allow https, even?)
  PRBool isHTTP = PR_FALSE, isHTTPS = PR_FALSE;
  iconURI->SchemeIs("http", &isHTTP);
  iconURI->SchemeIs("https", &isHTTPS);
  if (!isHTTP && !isHTTPS)
    return;
  
  nsCAutoString iconFullURI;
  iconURI->GetSpec(iconFullURI);
  NSString* iconSpec = [NSString stringWith_nsACString:iconFullURI];
  [mContainer onFoundShortcutIcon:iconSpec];
}

void
CHBrowserListener::HandleFeedLink(nsIDOMElement* inElement)
{
  //XXX Implement the new Firefox sniffing code. (nsIFeedSniffer)
  BOOL titleFound = YES;  // assume yes check below
  nsresult rv;
  
  nsCOMPtr<nsIDOMDocument> domDoc;
  inElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsAutoString titleAttr;
  rv = inElement->GetAttribute(NS_LITERAL_STRING("title"), titleAttr);
  if (NS_FAILED(rv))
    titleFound = NO;
    
  nsAutoString feedHref;
  rv = inElement->GetAttribute(NS_LITERAL_STRING("href"), feedHref);
  if (NS_FAILED(rv))
    return;

  // get the document uri
  nsCOMPtr<nsIDOM3Document> doc = do_QueryInterface(domDoc);
  if (!doc)
    return;

  nsAutoString docURISpec;
  rv = doc->GetDocumentURI(docURISpec);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIURI> documentURI;
  rv = NS_NewURI(getter_AddRefs(documentURI), docURISpec);
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIURI> feedURI;
  rv = NS_NewURI(getter_AddRefs(feedURI), NS_ConvertUTF16toUTF8(feedHref), nsnull, documentURI);
  if (NS_FAILED(rv))
    return;
  
  // set the scheme to feed:// so sending to outside application is one only one call
  feedURI->SetScheme(NS_LITERAL_CSTRING("feed"));
  
  nsCAutoString feedFullURI;
  feedURI->GetAsciiSpec(feedFullURI);
  
  // get the two specs, the feed's uri and the feed's title
  NSString* feedSpec = [NSString stringWith_nsACString:feedFullURI];
  NSString* titleSpec = nil;
  if (titleFound)
    titleSpec = [NSString stringWith_nsAString:titleAttr];
  
  // notify that a feed has sucessfully been discovered
  [mContainer onFeedDetected:feedSpec feedTitle:titleSpec];
}
