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

#import "NSBrowserView.h"
#import "nsCocoaBrowserService.h"

// Embedding includes
#include "nsCWebBrowser.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIURI.h"
#include "nsIDOMWindow.h"
#include "nsWeakReference.h"

// XPCOM and String includes
#include "nsCRT.h"
#include "nsString.h"
#include "nsCOMPtr.h"


class nsCocoaBrowserListener : public nsSupportsWeakReference,
                               public nsIInterfaceRequestor,
			       public nsIWebBrowserChrome,
			       public nsIEmbeddingSiteWindow,
                               public nsIWebProgressListener
{
public:
  nsCocoaBrowserListener(NSBrowserView* aView);
  virtual ~nsCocoaBrowserListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIWEBPROGRESSLISTENER

  void AddListener(id <NSBrowserListener> aListener);
  void RemoveListener(id <NSBrowserListener> aListener);
  void SetContainer(id <NSBrowserContainer> aContainer);

private:
  NSBrowserView* mView;     // WEAK - it owns us
  NSMutableArray* mListeners;
  id <NSBrowserContainer> mContainer;
  PRBool mIsModal;
  PRUint32 mChromeFlags;
};

nsCocoaBrowserListener::nsCocoaBrowserListener(NSBrowserView* aView)
  : mView(aView), mContainer(nsnull), mIsModal(PR_FALSE), mChromeFlags(0)
{
  mListeners = [[NSMutableArray alloc] init];
}

nsCocoaBrowserListener::~nsCocoaBrowserListener()
{
  [mListeners release];
  mView = nsnull;
  if (mContainer) {
    [mContainer release];
  }
}

NS_IMPL_ISUPPORTS5(nsCocoaBrowserListener,
		   nsIInterfaceRequestor,
		   nsIWebBrowserChrome,
		   nsIEmbeddingSiteWindow,
		   nsIWebProgressListener,
     		   nsISupportsWeakReference)

// Implementation of nsIInterfaceRequestor
NS_IMETHODIMP 
nsCocoaBrowserListener::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    nsCOMPtr<nsIWebBrowser> browser = dont_AddRef([mView getWebBrowser]);
    if (browser)
      return browser->GetContentDOMWindow((nsIDOMWindow **) aInstancePtr);
  }
  
  return QueryInterface(aIID, aInstancePtr);
}

// Implementation of nsIWebBrowserChrome
/* void setStatus (in unsigned long statusType, in wstring status); */
NS_IMETHODIMP 
nsCocoaBrowserListener::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* str = nsnull;
  if (status && (*status != PRUnichar(0))) {
    str = [NSString stringWithCharacters:status length:nsCRT::strlen(status)];
  }

  [mContainer setStatus:str ofType:(NSStatusType)statusType];

  return NS_OK;
}

/* attribute nsIWebBrowser webBrowser; */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  NS_ENSURE_ARG_POINTER(aWebBrowser);
  if (!mView) {
    return NS_ERROR_FAILURE;
  }
  *aWebBrowser = [mView getWebBrowser];

  return NS_OK;
}
NS_IMETHODIMP 
nsCocoaBrowserListener::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  [mView setWebBrowser:aWebBrowser];

  return NS_OK;
}

/* attribute unsigned long chromeFlags; */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetChromeFlags(PRUint32 *aChromeFlags)
{
  NS_ENSURE_ARG_POINTER(aChromeFlags);
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}
NS_IMETHODIMP 
nsCocoaBrowserListener::SetChromeFlags(PRUint32 aChromeFlags)
{
  // XXX Do nothing with them for now
  mChromeFlags = aChromeFlags;
  return NS_OK;
}

/* void destroyBrowserWindow (); */
NS_IMETHODIMP 
nsCocoaBrowserListener::DestroyBrowserWindow()
{
  // XXX Could send this up to the container, but for now,
  // we just destroy the enclosing window.
  NSWindow* window = [mView window];

  if (window) {
    [window close];
  }

  return NS_OK;
}

/* void sizeBrowserTo (in long aCX, in long aCY); */
NS_IMETHODIMP 
nsCocoaBrowserListener::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
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
nsCocoaBrowserListener::ShowAsModal()
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];

  if (!window) {
    return NS_ERROR_FAILURE;
  }

  mIsModal = PR_TRUE;
  int result = [NSApp runModalForWindow:window];
  mIsModal = PR_FALSE;

  return (nsresult)result;
}

/* boolean isWindowModal (); */
NS_IMETHODIMP 
nsCocoaBrowserListener::IsWindowModal(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mIsModal;

  return NS_OK;
}

/* void exitModalEventLoop (in nsresult aStatus); */
NS_IMETHODIMP 
nsCocoaBrowserListener::ExitModalEventLoop(nsresult aStatus)
{
  [NSApp stopModalWithCode:(int)aStatus];

  return NS_OK;
}

// Implementation of nsIEmbeddingSiteWindow
/* void setDimensions (in unsigned long flags, in long x, in long y, in long cx, in long cy); */
NS_IMETHODIMP 
nsCocoaBrowserListener::SetDimensions(PRUint32 flags, 
				      PRInt32 x, 
				      PRInt32 y, 
				      PRInt32 cx, 
				      PRInt32 cy)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    NSPoint origin;
    origin.x = (float)x;
    origin.y = (float)y;
    [window setFrameOrigin:origin];
  }

  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    NSRect frame = [window frame];
    frame.size.width = (float)cx;
    frame.size.height = (float)cy;
    [window setFrame:frame display:YES];
  }
  else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER) {
    NSSize size;
    size.width = (float)cx;
    size.height = (float)cy;
    [window setContentSize:size];
  }

  return NS_OK;
}

/* void getDimensions (in unsigned long flags, out long x, out long y, out long cx, out long cy); */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetDimensions(PRUint32 flags, 
				      PRInt32 *x, 
				      PRInt32 *y, 
				      PRInt32 *cx, 
				      PRInt32 *cy)
{
  NS_ENSURE_ARG_POINTER(x);
  NS_ENSURE_ARG_POINTER(y);
  NS_ENSURE_ARG_POINTER(cx);
  NS_ENSURE_ARG_POINTER(cy);

  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  NSRect frame = [window frame];
  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    *x = (PRInt32)frame.origin.x;
    *y = (PRInt32)frame.origin.y;
  }
  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    *cx = (PRInt32)frame.size.width;
    *cy = (PRInt32)frame.size.height;
  }
  else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER) {
    NSView* contentView = [window contentView];
    NSRect contentFrame = [contentView frame];
    *cx = (PRInt32)contentFrame.size.width;
    *cy = (PRInt32)contentFrame.size.height;    
  }

  return NS_OK;
}

/* void setFocus (); */
NS_IMETHODIMP 
nsCocoaBrowserListener::SetFocus()
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  [window makeKeyAndOrderFront:window];

  return NS_OK;
}

/* attribute boolean visibility; */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetVisibility(PRBool *aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  *aVisibility = [window isMiniaturized];

  return NS_OK;
}
NS_IMETHODIMP 
nsCocoaBrowserListener::SetVisibility(PRBool aVisibility)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  if (aVisibility) {
    [window deminiaturize:window];
  }
  else {
    [window miniaturize:window];
  }

  return NS_OK;
}

/* attribute wstring title; */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetTitle(PRUnichar * *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);

  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* title = [mContainer title];
  unsigned int length = [title length];
  if (length) {
    *aTitle = (PRUnichar*)nsMemory::Alloc((length+1)*sizeof(PRUnichar));
    if (!*aTitle) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    [title getCharacters:*aTitle];
  }
  else {
    *aTitle = nsnull;
  }
  
  return NS_OK;
}
NS_IMETHODIMP 
nsCocoaBrowserListener::SetTitle(const PRUnichar * aTitle)
{
  NS_ENSURE_ARG(aTitle);

  if (!mContainer) {
    return NS_ERROR_FAILURE;
  }

  NSString* str = [NSString stringWithCharacters:aTitle length:nsCRT::strlen(aTitle)];
  [mContainer setTitle:str];

  return NS_OK;
}

/* [noscript] readonly attribute voidPtr siteWindow; */
NS_IMETHODIMP 
nsCocoaBrowserListener::GetSiteWindow(void * *aSiteWindow)
{
  NS_ENSURE_ARG_POINTER(aSiteWindow);

  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  NSWindow* window = [mView window];
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  *aSiteWindow = (void*)window;

  return NS_OK;
}

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnStateChange(nsIWebProgress *aWebProgress, 
				      nsIRequest *aRequest, 
				      PRInt32 aStateFlags, 
				      PRUint32 aStatus)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id obj;
  
  if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) {
    if (aStateFlags & nsIWebProgressListener::STATE_START) {
      while ((obj = [enumerator nextObject])) {
	[obj onLoadingStarted];
      }
    }
    else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {
      while ((obj = [enumerator nextObject])) {
	[obj onLoadingCompleted:(NS_SUCCEEDED(aStatus))];
      }
    }
  }


  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnProgressChange(nsIWebProgress *aWebProgress, 
					 nsIRequest *aRequest, 
					 PRInt32 aCurSelfProgress, 
					 PRInt32 aMaxSelfProgress, 
					 PRInt32 aCurTotalProgress, 
					 PRInt32 aMaxTotalProgress)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id obj;
 
  while ((obj = [enumerator nextObject])) {
    [obj onProgressChange:aCurTotalProgress outOf:aMaxTotalProgress];
  }
  
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnLocationChange(nsIWebProgress *aWebProgress, 
					 nsIRequest *aRequest, 
					 nsIURI *location)
{
  nsCAutoString spec;
  
  location->GetAsciiSpec(spec);
  if (spec.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  const char* cstr = spec.get();
  NSString* str = [NSString stringWithCString:cstr];
  NSURL* url = [NSURL URLWithString:str];

  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id obj;
 
  while ((obj = [enumerator nextObject])) {
    [obj onLocationChange:url];
  }

  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnStatusChange(nsIWebProgress *aWebProgress, 
				       nsIRequest *aRequest, 
				       nsresult aStatus, 
				       const PRUnichar *aMessage)
{
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnSecurityChange(nsIWebProgress *aWebProgress, 
					 nsIRequest *aRequest, 
					 PRInt32 state)
{
  return NS_OK;
}

void 
nsCocoaBrowserListener::AddListener(id <NSBrowserListener> aListener)
{
  [mListeners addObject:aListener];
}

void 
nsCocoaBrowserListener::RemoveListener(id <NSBrowserListener> aListener)
{
  [mListeners removeObject:aListener];
}

void 
nsCocoaBrowserListener::SetContainer(id <NSBrowserContainer> aContainer)
{
  [mContainer autorelease];

  mContainer = aContainer;

  [mContainer retain];
}


@implementation NSBrowserView

- (id)initWithFrame:(NSRect)frame
{
  [super initWithFrame:frame];

  nsresult rv = nsCocoaBrowserService::InitEmbedding();
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }
  
  _listener = new nsCocoaBrowserListener(self);
  NS_ADDREF(_listener);

  // Create the web browser instance
  nsCOMPtr<nsIWebBrowser> browser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }

  _webBrowser = browser;
  NS_ADDREF(_webBrowser);

  // Set the container nsIWebBrowserChrome
  _webBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, 
						 _listener));
  
  // Register as a listener for web progress
  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, _listener));
  _webBrowser->AddWebBrowserListener(weak, NS_GET_IID(nsIWebProgressListener));

  // Hook up the widget hierarchy with us as the parent
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(_webBrowser);
  baseWin->InitWindow((NSView*)self, nsnull, 0, 0, 
		      frame.size.width, frame.size.height);
  baseWin->Create();

  return self;
}

- (void)dealloc 
{
  NS_RELEASE(_listener);
  NS_IF_RELEASE(_webBrowser);
  nsCocoaBrowserService::TermEmbedding();

  [super dealloc];
}

- (void)setFrame:(NSRect)frameRect 
{
  [super setFrame:frameRect];
  if (_webBrowser) {
    nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(_webBrowser);
    window->SetSize((PRInt32)frameRect.size.width, 
		    (PRInt32)frameRect.size.height,
		    PR_TRUE);
  }
}

- (void)addListener:(id <NSBrowserListener>)listener
{
  _listener->AddListener(listener);
}

- (void)removeListener:(id <NSBrowserListener>)listener
{
  _listener->RemoveListener(listener);
}

- (void)setContainer:(id <NSBrowserContainer>)container
{
  _listener->SetContainer(container);
}

- (nsIDOMWindow*)getContentWindow
{
  nsIDOMWindow* window;

  _webBrowser->GetContentDOMWindow(&window);

  return window;
}

- (void)loadURI:(NSURL *)url flags:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  
  NSString* spec = [url absoluteString];
  int length = [spec length];
  PRUnichar* specStr = nsMemory::Alloc((length+1) * sizeof(PRUnichar));
  [spec getCharacters:specStr];
  specStr[length] = PRUnichar(0);
  

  PRUint32 navFlags = nsIWebNavigation::LOAD_FLAGS_NONE;
  if (flags & NSLoadFlagsDontPutInHistory) {
    navFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY;
  }
  if (flags & NSLoadFlagsReplaceHistoryEntry) {
    navFlags |= nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY;
  }
  if (flags & NSLoadFlagsBypassCacheAndProxy) {
    navFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | 
                nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
  }

  nsresult rv = nav->LoadURI(specStr, navFlags, nsnull, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }

  nsMemory::Free(specStr);
}

- (void)reload:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  PRUint32 navFlags = nsIWebNavigation::LOAD_FLAGS_NONE;
  if (flags & NSLoadFlagsBypassCacheAndProxy) {
    navFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | 
                nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
  }

  nsresult rv = nav->Reload(navFlags);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }  
}

- (BOOL)canGoBack
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  PRBool can;
  nav->GetCanGoBack(&can);

  return can ? YES : NO;
}

- (BOOL)canGoForward
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  PRBool can;
  nav->GetCanGoForward(&can);

  return can ? YES : NO;
}

- (void)goBack
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nsresult rv = nav->GoBack();
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }  
}

- (void)goForward
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nsresult rv = nav->GoForward();
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }  
}

- (void)gotoIndex:(int)index
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nsresult rv = nav->GotoIndex(index);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }    
}

- (void)stop:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nsresult rv = nav->Stop(flags);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }    
}

- (NSURL*)getCurrentURI
{
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nav->GetCurrentURI(getter_AddRefs(uri));
  if (!uri) {
    return nsnull;
  }

  nsCAutoString spec;
  uri->GetAsciiSpec(spec);
  
  const char* cstr = spec.get();
  NSString* str = [NSString stringWithCString:cstr];
  NSURL* url = [NSURL URLWithString:str];
  
  return url;
}

- (nsIWebBrowser*)getWebBrowser
{
  NS_IF_ADDREF(_webBrowser);
  return _webBrowser;
}

- (void)setWebBrowser:(nsIWebBrowser*)browser
{
  _webBrowser = browser;

  if (_webBrowser) {
    // Set the container nsIWebBrowserChrome
    _webBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, 
						   _listener));

    NSRect frame = [self frame];
 
    // Hook up the widget hierarchy with us as the parent
    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(_webBrowser);
    baseWin->InitWindow((NSView*)self, nsnull, 0, 0, 
			frame.size.width, frame.size.height);
    baseWin->Create();
  }

}

@end

