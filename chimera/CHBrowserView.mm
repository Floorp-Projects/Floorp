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

#import "CHBrowserView.h"
#import "ProgressDlgController.h"
#import "FindDlgController.h"
#import "nsCocoaBrowserService.h"
#import "mozView.h"

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
#include "nsIWidget.h"

// XPCOM and String includes
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

// Printing
#include "nsIWebBrowserPrint.h"
#include "nsIPrintSettings.h"

// Saving of links/images/docs
#include "nsIWebBrowserFocus.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIURL.h"
#include "nsIWebBrowserPersist.h"
#include "nsIProperties.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIPref.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "nsISHistory.h"
#include "nsIHistoryEntry.h"
#include "nsISHEntry.h"
#include "nsNetUtil.h"
#include "nsIContextMenuListener.h"
#include "nsITooltipListener.h"
#include "nsIEmbeddingSiteWindow2.h"

typedef unsigned int DragReference;
#include "nsIDragHelperService.h"

// Cut/copy/paste
#include "nsIClipboardCommands.h"
#include "nsIInterfaceRequestorUtils.h"

// Undo/redo
#include "nsICommandManager.h"
#include "nsICommandParams.h"

const char* persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
const char* dirServiceContractID = "@mozilla.org/file/directory_service;1";

class nsCocoaBrowserListener : public nsSupportsWeakReference,
                               public nsIInterfaceRequestor,
                               public nsIWebBrowserChrome,
                               public nsIWindowCreator,
                               public nsIEmbeddingSiteWindow2,
                               public nsIWebProgressListener,
                               public nsIContextMenuListener,
                               public nsITooltipListener
{
public:
  nsCocoaBrowserListener(CHBrowserView* aView);
  virtual ~nsCocoaBrowserListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWINDOWCREATOR
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIEMBEDDINGSITEWINDOW2
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSICONTEXTMENULISTENER
  NS_DECL_NSITOOLTIPLISTENER
    
  void AddListener(id <NSBrowserListener> aListener);
  void RemoveListener(id <NSBrowserListener> aListener);
  void SetContainer(id <NSBrowserContainer> aContainer);

private:
  CHBrowserView* mView;     // WEAK - it owns us
  NSMutableArray* mListeners;
  id <NSBrowserContainer> mContainer;
  PRBool mIsModal;
  PRUint32 mChromeFlags;
};

nsCocoaBrowserListener::nsCocoaBrowserListener(CHBrowserView* aView)
  : mView(aView), mContainer(nsnull), mIsModal(PR_FALSE), mChromeFlags(0)
{
  NS_INIT_ISUPPORTS();
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

NS_IMPL_ISUPPORTS9(nsCocoaBrowserListener,
                   nsIInterfaceRequestor,
                   nsIWebBrowserChrome,
                   nsIWindowCreator,
                   nsIEmbeddingSiteWindow,
                   nsIEmbeddingSiteWindow2,
                   nsIWebProgressListener,
                   nsISupportsWeakReference,
                   nsIContextMenuListener,
                   nsITooltipListener)

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

// Implementation of nsIWindowCreator.  The CocoaBrowserService forwards requests
// for a new window that have a parent to us, and we take over from there.  
/* nsIWebBrowserChrome createChromeWindow (in nsIWebBrowserChrome parent, in PRUint32 chromeFlags); */
NS_IMETHODIMP 
nsCocoaBrowserListener::CreateChromeWindow(nsIWebBrowserChrome *parent, 
                                           PRUint32 chromeFlags, 
                                           nsIWebBrowserChrome **_retval)
{
  if (parent != this) {
#if DEBUG
    NSLog(@"Mismatch in nsCocoaBrowserListener::CreateChromeWindow.  We should be the owning parent.");
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
  
  nsCocoaBrowserListener* listener = [childView getCocoaBrowserListener];
  if (!listener) {
#if DEBUG
    NSLog(@"Uh-oh! No listener yet for a newly created window (nsCocoaBrowserlistener)");
    return NS_ERROR_FAILURE;
#endif
  }
  
#if DEBUG
  NSLog(@"Made a chrome window.");
#endif
  
  *_retval = listener;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

// Implementation of nsIContextMenuListener
NS_IMETHODIMP
nsCocoaBrowserListener::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent* aEvent, nsIDOMNode* aNode)
{
  [mContainer onShowContextMenu: aContextFlags domEvent: aEvent domNode: aNode];
  return NS_OK;
}

// Implementation of nsITooltipListener
NS_IMETHODIMP
nsCocoaBrowserListener::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
  NSPoint where;
  where.x = aXCoords; where.y = aYCoords;
  [mContainer onShowTooltip:where withText:[NSString stringWithCharacters:aTipText length:nsCRT::strlen(aTipText)]];
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserListener::OnHideTooltip()
{
  [mContainer onHideTooltip];
  return NS_OK;
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
  //int result = [NSApp runModalForWindow:window];
  mIsModal = PR_FALSE;

  return NS_OK;
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
//  [NSApp stopModalWithCode:(int)aStatus];

  return NS_OK;
}

// Implementation of nsIEmbeddingSiteWindow2
NS_IMETHODIMP
nsCocoaBrowserListener::Blur()
{
  return NS_OK;
}

// Implementation of nsIEmbeddingSiteWindow
/* void setDimensions (in unsigned long flags, in long x, in long y, in long cx, in long cy); */
NS_IMETHODIMP 
nsCocoaBrowserListener::SetDimensions(PRUint32 flags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
  if (!mView)
    return NS_ERROR_FAILURE;

  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

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
nsCocoaBrowserListener::GetDimensions(PRUint32 flags,  PRInt32 *x,  PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
  if (!mView)
    return NS_ERROR_FAILURE;

  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

  NSRect frame = [window frame];
  if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    if ( x )
      *x = (PRInt32)frame.origin.x;
    if ( y )
      *y = (PRInt32)frame.origin.y;
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
  *aSiteWindow = nsnull;
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


//
// Implementation of nsIWebProgressListener
//

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                        PRUint32 aStateFlags, PRUint32 aStatus)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<NSBrowserListener> obj;
  
  if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) {
    if (aStateFlags & nsIWebProgressListener::STATE_START) {
      while ((obj = [enumerator nextObject]))
        [obj onLoadingStarted];
    }
    else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {
      while ((obj = [enumerator nextObject]))
        [obj onLoadingCompleted:(NS_SUCCEEDED(aStatus))];
    }
  }

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                          PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
                                          PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<NSBrowserListener> obj;
  while ((obj = [enumerator nextObject]))
    [obj onProgressChange:aCurTotalProgress outOf:aMaxTotalProgress];
  
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
                                          nsIURI *location)
{
  if (!location)
    return NS_ERROR_FAILURE;
    
  nsCAutoString spec;
  location->GetSpec(spec);
  NSString* str = [NSString stringWithCString:spec.get()];

  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<NSBrowserListener> obj;
  while ((obj = [enumerator nextObject]))
    [obj onLocationChange:str];

  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, 
                                        const PRUnichar *aMessage)
{
  NSString* str = [NSString stringWithCharacters:aMessage length:nsCRT::strlen(aMessage)];
  
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<NSBrowserListener> obj; 
  while ((obj = [enumerator nextObject]))
    [obj onStatusChange: str];

  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsCocoaBrowserListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  NSEnumerator* enumerator = [mListeners objectEnumerator];
  id<NSBrowserListener> obj; 
  while ((obj = [enumerator nextObject]))
    [obj onSecurityStateChange: state];

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

// Implementation of a header sniffer class that is used when saving Web pages and images.
class nsHeaderSniffer :  public nsIWebProgressListener
{
public:
    nsHeaderSniffer(nsIWebBrowserPersist* aPersist, nsIFile* aFile, nsIURI* aURL,
                    nsIDOMDocument* aDocument, nsIInputStream* aPostData,
                    const nsCString& aSuggestedFilename, PRBool aBypassCache,
                    NSView* aFilterView, NSPopUpButton* aFilterList)
    {
        NS_INIT_REFCNT();
        mPersist = aPersist;
        mTmpFile = aFile;
        mURL = aURL;
        mDocument = aDocument;
        mPostData = aPostData;
        mDefaultFilename = aSuggestedFilename;
        mBypassCache = aBypassCache;
        mFilterView = aFilterView;
        mFilterList = aFilterList;
    }
                  
    virtual ~nsHeaderSniffer() 
    {
    };

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
  
protected:
    void PerformSave();
    
private:
    nsIWebBrowserPersist* mPersist; // Weak. It owns us as a listener.
    nsCOMPtr<nsIFile> mTmpFile;
    nsCOMPtr<nsIURI> mURL;
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIInputStream> mPostData;
    nsCString mDefaultFilename;
    PRBool mBypassCache;
    nsCString mContentType;
    nsCString mContentDisposition;
    NSView* mFilterView;
    NSPopUpButton* mFilterList;
};

NS_IMPL_ISUPPORTS1(nsHeaderSniffer, nsIWebProgressListener)

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsHeaderSniffer::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                PRUint32 aStatus)
{
    if (aStateFlags & nsIWebProgressListener::STATE_START) {
        nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
        channel->GetContentType(mContentType);
       
        // Get the content-disposition if we're an HTTP channel.
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
        if (httpChannel)
            httpChannel->GetResponseHeader(nsCAutoString("content-disposition"), mContentDisposition);
        
        mPersist->CancelSave();
        PRBool exists;
        mTmpFile->Exists(&exists);
        if (exists)
            mTmpFile->Remove(PR_FALSE);
        PerformSave();
    }
    return NS_OK;
}

void nsHeaderSniffer::PerformSave()
{
    // Are we an HTML document? If so, we will want to append an accessory view to
    // the save dialog to provide the user with the option of doing a complete
    // save vs. a single file save.
    PRBool isHTML = (mDocument && mContentType.Equals("text/html") ||
                     mContentType.Equals("text/xml") ||
                     mContentType.Equals("application/xhtml+xml"));
    
    // Next find out the directory that we should start in.
    nsCOMPtr<nsIPrefService> prefs(do_GetService("@mozilla.org/preferences-service;1"));
    if (!prefs)
        return;
    nsCOMPtr<nsIPrefBranch> dirBranch;
    prefs->GetBranch("browser.download.", getter_AddRefs(dirBranch));
    PRInt32 filterIndex = 0;
    if (dirBranch) {
        nsresult rv = dirBranch->GetIntPref("save_converter_index", &filterIndex);
        if (NS_FAILED(rv))
            filterIndex = 0;
    }
    if (mFilterList)
        [mFilterList selectItemAtIndex: filterIndex];
        
    // We need to figure out what file name to use.
    nsCAutoString defaultFileName;
    if (!mContentDisposition.IsEmpty()) {
        // (1) Use the HTTP header suggestion.
        PRInt32 index = mContentDisposition.Find("filename=");
        if (index >= 0) {
            // Take the substring following the prefix.
            index += 9;
            nsCAutoString filename;
            mContentDisposition.Right(filename, mContentDisposition.Length() - index);
            defaultFileName = filename;
        }
    }
    
    if (defaultFileName.IsEmpty()) {
        nsCOMPtr<nsIURL> url(do_QueryInterface(mURL));
        if (url)
            url->GetFileName(defaultFileName); // (2) For file URLs, use the file name.
    }
    
    if (defaultFileName.IsEmpty() && mDocument && isHTML) {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
        nsAutoString title;
        if (htmlDoc)
            htmlDoc->GetTitle(title); // (3) Use the title of the document.
        defaultFileName.AssignWithConversion(title);
    }
    
    if (defaultFileName.IsEmpty()) {
        // (4) Use the caller provided name.
        defaultFileName = mDefaultFilename;
    }

    if (defaultFileName.IsEmpty() && mURL)
        // (5) Use the host.
        mURL->GetHost(defaultFileName);
    
    // One last case to handle about:blank and other fruity untitled pages.
    if (defaultFileName.IsEmpty())
        defaultFileName = "untitled";
        
    // Validate the file name to ensure legality.
    for (PRUint32 i = 0; i < defaultFileName.Length(); i++)
        if (defaultFileName[i] == ':' || defaultFileName[i] == '/')
            defaultFileName.SetCharAt(i, ' ');
            
    // Make sure the appropriate extension is appended to the suggested file name.
    nsCOMPtr<nsIURI> fileURI(do_CreateInstance("@mozilla.org/network/standard-url;1"));
    nsCOMPtr<nsIURL> fileURL(do_QueryInterface(fileURI));
    if (!fileURL)
        return;
    fileURL->SetFilePath(defaultFileName);
    
    nsCAutoString fileExtension;
    fileURL->GetFileExtension(fileExtension);
    
    PRBool setExtension = PR_FALSE;
    if (mContentType.Equals("text/html")) {
        if (fileExtension.IsEmpty() || (!fileExtension.Equals("htm") && !fileExtension.Equals("html"))) {
            defaultFileName += ".html";
            setExtension = PR_TRUE;
        }
    }
    
    if (!setExtension && fileExtension.IsEmpty()) {
        nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1"));
        if (!mimeService)
            return;
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        mimeService->GetFromMIMEType(mContentType.get(), getter_AddRefs(mimeInfo));
        if (!mimeInfo)
          return;

        PRUint32 extCount = 0;
        char** extList = nsnull;
        mimeInfo->GetFileExtensions(&extCount, &extList);        
        if (extCount > 0 && extList) {
            defaultFileName += ".";
            defaultFileName += extList[0];
        }
    }
    
    // Now it's time to pose the save dialog.
    NSSavePanel* savePanel = [NSSavePanel savePanel];
    NSString* file = nil;
    if (!defaultFileName.IsEmpty())
        file = [[NSString alloc] initWithCString: defaultFileName.get()];
        
    if (isHTML)
        [savePanel setAccessoryView: mFilterView];
        
    if ([savePanel runModalForDirectory: nil file: file] == NSFileHandlingPanelCancelButton)
        return;
       
    // Release the file string.
    [file release];
    
    // Update the filter index.
    if (isHTML && mFilterList) {
        filterIndex = [mFilterList indexOfSelectedItem];
        dirBranch->SetIntPref("save_converter_index", filterIndex);
    }
    
    // Convert the content type to text/plain if it was selected in the filter.
    if (isHTML && filterIndex == 2)
        mContentType = "text/plain";
    
    nsCOMPtr<nsISupports> sourceData;
    if (isHTML && filterIndex != 1)
        sourceData = do_QueryInterface(mDocument);
    else
        sourceData = do_QueryInterface(mURL);
        
    nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(persistContractID));
    ProgressDlgController* progressDialog = [[ProgressDlgController alloc] initWithWindowNibName: @"ProgressDialog"];
    [progressDialog setWebPersist: webPersist 
                    source: sourceData.get()
                    destination: [savePanel filename]
                    contentType: mContentType.get()
                    postData: mPostData
                    bypassCache: mBypassCache];
                    
    [progressDialog showWindow: progressDialog];
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsHeaderSniffer::OnProgressChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           PRInt32 aCurSelfProgress, 
           PRInt32 aMaxSelfProgress, 
           PRInt32 aCurTotalProgress, 
           PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsHeaderSniffer::OnLocationChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           nsIURI *location)
{
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsHeaderSniffer::OnStatusChange(nsIWebProgress *aWebProgress, 
               nsIRequest *aRequest, 
               nsresult aStatus, 
               const PRUnichar *aMessage)
{
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsHeaderSniffer::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  return NS_OK;
}


@implementation CHBrowserView

- (id)initWithFrame:(NSRect)frame andWindow:(NSWindow*)aWindow
{
  mWindow = aWindow;
  return [self initWithFrame:frame];
}

- (id)initWithFrame:(NSRect)frame
{
    if ( (self = [super initWithFrame:frame]) ) {

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
        
  // register the view as a drop site for text, files, and urls. 
        [self registerForDraggedTypes:
                [NSArray arrayWithObjects:NSStringPboardType, NSURLPboardType, NSFilenamesPboardType, nil]];
    }
  return self;
}

- (void)destroyWebBrowser
{
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(_webBrowser);
  baseWin->Destroy();
}

- (void)dealloc 
{
  [super dealloc];
  
  NS_RELEASE(_listener);
  NS_IF_RELEASE(_webBrowser);
  
  nsCocoaBrowserService::BrowserClosed();
  
#if DEBUG
  NSLog(@"CHBrowserView died.");
#endif
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

- (void)loadURI:(NSString *)urlSpec flags:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  
  int length = [urlSpec length];
  PRUnichar* specStr = nsMemory::Alloc((length+1) * sizeof(PRUnichar));
  [urlSpec getCharacters:specStr];
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

// XXXbryner This isn't used anywhere. how is it different from getCurrentURLSpec?
- (NSString*)getCurrentURI
{
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);

  nav->GetCurrentURI(getter_AddRefs(uri));
  if (!uri) {
    return nsnull;
  }

  nsCAutoString spec;
  uri->GetSpec(spec);
  
  const char* cstr = spec.get();
  NSString* str = [NSString stringWithCString:cstr];
  
  return str;
}

- (nsCocoaBrowserListener*)getCocoaBrowserListener
{
  return _listener;
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

-(void) saveInternal: (nsIURI*)aURI
        withDocument: (nsIDOMDocument*)aDocument
        suggestedFilename: (const char*)aFilename
        bypassCache: (BOOL)aBypassCache
        filterView: (NSView*)aFilterView
        filterList: (NSPopUpButton*)aFilterList
{
    // Create our web browser persist object.  This is the object that knows
    // how to actually perform the saving of the page (and of the images
    // on the page).
    nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(persistContractID));
    if (!webPersist)
        return;
    
    // Make a temporary file object that we can save to.
    nsCOMPtr<nsIProperties> dirService(do_GetService(dirServiceContractID));
    if (!dirService)
        return;
    nsCOMPtr<nsIFile> tmpFile;
    dirService->Get("TmpD", NS_GET_IID(nsIFile), getter_AddRefs(tmpFile));
    static short unsigned int tmpRandom = 0;
    nsAutoString tmpNo; tmpNo.AppendInt(tmpRandom++);
    nsAutoString saveFile(NS_LITERAL_STRING("-sav"));
    saveFile += tmpNo;
    saveFile += NS_LITERAL_STRING("tmp");
    tmpFile->Append(saveFile); 
    
    // Get the post data if we're an HTML doc.
    nsCOMPtr<nsIInputStream> postData;
    if (aDocument) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(_webBrowser));
      nsCOMPtr<nsISHistory> sessionHistory;
      webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
      nsCOMPtr<nsIHistoryEntry> entry;
      PRInt32 sindex;
      sessionHistory->GetIndex(&sindex);
      sessionHistory->GetEntryAtIndex(sindex, PR_FALSE, getter_AddRefs(entry));
      nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(entry));
      if (shEntry)
          shEntry->GetPostData(getter_AddRefs(postData));
    }

    nsCAutoString fileName(aFilename);
    nsHeaderSniffer* sniffer = new nsHeaderSniffer(webPersist, tmpFile, aURI, 
                                                   aDocument, postData, fileName, aBypassCache,
                                                   aFilterView, aFilterList);
    if (!sniffer)
        return;
    webPersist->SetProgressListener(sniffer);
    webPersist->SaveURI(aURI, nsnull, tmpFile);
}

-(void)printDocument
{
    nsCOMPtr<nsIDOMWindow> domWindow;
    _webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    nsCOMPtr<nsIInterfaceRequestor> ir(do_QueryInterface(domWindow));
    nsCOMPtr<nsIWebBrowserPrint> print;
    ir->GetInterface(NS_GET_IID(nsIWebBrowserPrint), getter_AddRefs(print));
    print->Print(nsnull, nsnull);
}

- (BOOL)findInPageWithPattern:(NSString*)inText caseSensitive:(BOOL)inCaseSensitive
    wrap:(BOOL)inWrap backwards:(BOOL)inBackwards
{
    PRBool found =  PR_FALSE;
    
    nsCOMPtr<nsIWebBrowserFocus> wbf(do_QueryInterface(_webBrowser));
    nsCOMPtr<nsIDOMWindow> rootWindow;
    nsCOMPtr<nsIDOMWindow> focusedWindow;
    _webBrowser->GetContentDOMWindow(getter_AddRefs(rootWindow));
    wbf->GetFocusedWindow(getter_AddRefs(focusedWindow));
    if (!focusedWindow)
        focusedWindow = rootWindow;
    nsCOMPtr<nsIWebBrowserFind> webFind(do_GetInterface(_webBrowser));
    if ( webFind ) {
      nsCOMPtr<nsIWebBrowserFindInFrames> framesFind(do_QueryInterface(webFind));
      framesFind->SetRootSearchFrame(rootWindow);
      framesFind->SetCurrentSearchFrame(focusedWindow);
      
      webFind->SetMatchCase(inCaseSensitive ? PR_TRUE : PR_FALSE);
      webFind->SetWrapFind(inWrap ? PR_TRUE : PR_FALSE);
      webFind->SetFindBackwards(inBackwards ? PR_TRUE : PR_FALSE);
    
      PRUnichar* text = (PRUnichar*)nsMemory::Alloc(([inText length]+1)*sizeof(PRUnichar));
      if ( text ) {
        [inText getCharacters:text];
        text[[inText length]] = 0;
        webFind->SetSearchString(text);
        webFind->FindNext(&found);
        nsMemory::Free(text);
      }
    }
    return found;
}

- (void)saveURL: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
            url: (NSString*)aURLSpec suggestedFilename: (NSString*)aFilename
{
  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), [aURLSpec cString]);
  if (NS_FAILED(rv))
    return;
  
  [self saveInternal: url.get()
        withDocument: nsnull
   suggestedFilename: [aFilename cString]
         bypassCache: YES
          filterView: aFilterView
          filterList: aFilterList];
}

-(NSString*)getFocusedURLString
{
  nsCOMPtr<nsIWebBrowserFocus> wbf(do_QueryInterface(_webBrowser));
  nsCOMPtr<nsIDOMWindow> domWindow;
  wbf->GetFocusedWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    _webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    return @"";

  nsCOMPtr<nsIDOMDocument> domDocument;
  domWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return @"";
  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDocument));
  if (!nsDoc)
    return @"";
  nsCOMPtr<nsIDOMLocation> location;
  nsDoc->GetLocation(getter_AddRefs(location));
  if (!location)
    return @"";
  nsAutoString urlStr;
  location->GetHref(urlStr);
  return [NSString stringWithCharacters: urlStr.get() length: urlStr.Length()];
}

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
{
    nsCOMPtr<nsIWebBrowserFocus> wbf(do_QueryInterface(_webBrowser));
    nsCOMPtr<nsIDOMWindow> domWindow;
    wbf->GetFocusedWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        _webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        return;
    
    nsCOMPtr<nsIDOMDocument> domDocument;
    domWindow->GetDocument(getter_AddRefs(domDocument));
    if (!domDocument)
        return;
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDocument));
    if (!nsDoc)
        return;
    nsCOMPtr<nsIDOMLocation> location;
    nsDoc->GetLocation(getter_AddRefs(location));
    if (!location)
        return;
    nsAutoString urlStr;
    location->GetHref(urlStr);
    nsCAutoString urlCStr; urlCStr.AssignWithConversion(urlStr);
    nsCOMPtr<nsIURI> url;
    nsresult rv = NS_NewURI(getter_AddRefs(url), urlCStr.get());
    if (NS_FAILED(rv))
        return;
    
    [self saveInternal: url.get()
          withDocument: domDocument
          suggestedFilename: ""
          bypassCache: NO
          filterView: aFilterView
          filterList: aFilterList];
}

-(void)doCommand:(const char*)commandName
{
  nsCOMPtr<nsICommandManager> commandMgr(do_GetInterface(_webBrowser));
  if (commandMgr) {
    nsCOMPtr<nsICommandParams> commandParams = do_CreateInstance("@mozilla.org/embedcomp/command-params;1");
    if (commandParams) {
      nsresult rv;
      
      nsAutoString commandNameStr;
      commandNameStr.AssignWithConversion(commandName);
      
      rv = commandParams->SetStringValue(NS_LITERAL_STRING("cmd_name"), commandNameStr);
      rv = commandMgr->DoCommand(commandParams);
#if DEBUG
      if (NS_FAILED(rv))
        NSLog(@"DoCommand failed");
#endif
    }
    else {
#if DEBUG
      NSLog(@"Failed to make command params");
#endif
    }
  }
  else {
#if DEBUG
    NSLog(@"No command manager");
#endif
  }
}

-(BOOL)isCommandEnabled:(const char*)commandName
{
  PRBool	isEnabled = PR_FALSE;
  nsCOMPtr<nsICommandManager> commandMgr(do_GetInterface(_webBrowser));
  if (commandMgr) {
    nsAutoString commandNameStr;
    commandNameStr.AssignWithConversion(commandName);
    nsresult rv = commandMgr->IsCommandEnabled(commandNameStr, &isEnabled);
#if DEBUG
    if (NS_FAILED(rv))
      NSLog(@"IsCommandEnabled failed");
#endif
  }
  else {
#if DEBUG
    NSLog(@"No command manager");
#endif
  }
  
  return (isEnabled) ? YES : NO;
}


-(IBAction)cut:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->CutSelection();
}

-(BOOL)canCut
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->CanCutSelection(&canCut);
  return canCut;
}

-(IBAction)copy:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->CopySelection();
}

-(BOOL)canCopy
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->CanCopySelection(&canCut);
  return canCut;
}

-(IBAction)paste:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->Paste();
}

-(BOOL)canPaste
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->CanPaste(&canCut);
  return canCut;
}

-(IBAction)delete:(id)aSender
{
  [self doCommand: "cmd_delete"];
}

-(BOOL)canDelete
{
  return [self isCommandEnabled: "cmd_delete"];
}

-(IBAction)selectAll:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  clipboard->SelectAll();
}

-(IBAction)undo:(id)aSender
{
  [self doCommand: "cmd_undo"];
}

-(IBAction)redo:(id)aSender
{
  [self doCommand: "cmd_redo"];
}

- (BOOL)canUndo
{
  return [self isCommandEnabled: "cmd_undo"];
}

- (BOOL)canRedo
{
  return [self isCommandEnabled: "cmd_redo"];
}

-(NSString*)getCurrentURLSpec
{
    NSString* empty = @"";
    nsCOMPtr<nsIDOMWindow> domWindow;
    _webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        return empty;
    
    nsCOMPtr<nsIDOMDocument> domDocument;
    domWindow->GetDocument(getter_AddRefs(domDocument));
    if (!domDocument)
        return empty;
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDocument));
    if (!nsDoc)
        return empty;
    nsCOMPtr<nsIDOMLocation> location;
    nsDoc->GetLocation(getter_AddRefs(location));
    if (!location)
        return empty;
    nsAutoString urlStr;
    location->GetHref(urlStr);
    nsCAutoString urlCStr; urlCStr.AssignWithConversion(urlStr);
    
    return [NSString stringWithCString: urlCStr.get()];
}

- (void)setActive: (BOOL)aIsActive
{
    nsCOMPtr<nsIWebBrowserFocus> wbf(do_QueryInterface(_webBrowser));
    if (aIsActive)
        wbf->Activate();
    else
        wbf->Deactivate();
}

-(NSMenu*)getContextMenu
{
  return [[self superview] getContextMenu];
}

-(NSWindow*)getNativeWindow
{
  NSWindow* result = [self window];
  if (result)
    return result; // We're visible.  Just hand the window back.
  else {
    // We're invisible.  It's likely that we're in a Cocoa tab view.
    // First see if we have a cached window.
    if (mWindow)
      return mWindow;
    
    // Finally, see if our parent responds to the getNativeWindow selector,
    // and if they do, let them handle it.
    return [[self superview] getNativeWindow];
  }
}


//
// -findEventSink:forPoint:inWindow:
//
// Given a point in window coordinates, find the Gecko event sink of the ChildView
// the point is over. This involves first converting the point to this view's
// coordinate system and using hitTest: to get the subview. Then we get
// that view's widget and QI it to an event sink
//
- (void) findEventSink:(nsIEventSink**)outSink forPoint:(NSPoint)inPoint inWindow:(NSWindow*)inWind
{
  NSPoint localPoint = [self convertPoint:inPoint fromView:[inWind contentView]];
  NSView<mozView>* hitView = [self hitTest:localPoint];
  if ( [hitView conformsToProtocol:@protocol(mozView)] ) {
    nsCOMPtr<nsIEventSink> sink (do_QueryInterface([hitView widget]));
    *outSink = sink.get();
    NS_IF_ADDREF(*outSink);
  }
}


- (unsigned int)draggingEntered:(id <NSDraggingInfo>)sender
{
  nsCOMPtr<nsIDragHelperService> helper(do_GetService("@mozilla.org/widget/draghelperservice;1"));
  mDragHelper = helper.get();
  NS_IF_ADDREF(mDragHelper);
  NS_ASSERTION ( mDragHelper, "Couldn't get a drag service, we're in biiig trouble" );
  
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:[sender draggingLocation]
            inWindow:[sender draggingDestinationWindow]];
    NS_ASSERTION(sink, "Couldn't get event sink for view");

    mDragHelper->Enter ( [sender draggingSequenceNumber], sink );
  }
  
  return NSDragOperationCopy;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:[sender draggingLocation]
            inWindow:[sender draggingDestinationWindow]];
    NS_ASSERTION(sink, "Couldn't get event sink for view");

    mDragHelper->Leave ( [sender draggingSequenceNumber], sink );
    NS_RELEASE(mDragHelper);     
  }
}

- (unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  PRBool dropAllowed = PR_FALSE;
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:[sender draggingLocation]
            inWindow:[sender draggingDestinationWindow]];
    NS_ASSERTION(sink, "Couldn't get event sink for view");
    
    mDragHelper->Tracking ( [sender draggingSequenceNumber], sink, &dropAllowed );
  }
  
  return dropAllowed ? NSDragOperationCopy : NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
  return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  PRBool dragAccepted = PR_FALSE;
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:[sender draggingLocation]
            inWindow:[sender draggingDestinationWindow]];
    NS_ASSERTION(sink, "Couldn't get event sink for view");

    mDragHelper->Drop ( [sender draggingSequenceNumber], sink, &dragAccepted );
  }
  
  return dragAccepted;
}


-(BOOL)validateMenuItem: (NSMenuItem*)aMenuItem
{
  // update first responder items based on the selection
  SEL action = [aMenuItem action];
  if (action == @selector(cut:))
    return [self canCut];    
  else if (action == @selector(copy:))
    return [self canCopy];
  else if (action == @selector(paste:))
    return [self canPaste];
  else if (action == @selector(delete:))
    return [self canDelete];
  else if (action == @selector(undo:))
    return [self canUndo];
  else if (action == @selector(redo:))
    return [self canRedo];
  else if (action == @selector(selectAll:))
    return YES;
  
  return NO;
}

@end

