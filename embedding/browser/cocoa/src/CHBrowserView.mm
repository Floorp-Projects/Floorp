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

#import "NSString+Utils.h"
#import "CHClickListener.h"

#include "nsCWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsComponentManagerUtils.h"

#include "nsIURI.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMEventReceiver.h"
#include "nsIWidget.h"

// Printing
#include "nsIWebBrowserPrint.h"
//#include "nsIPrintSettings.h"

// Saving of links/images/docs
#include "nsIWebBrowserFocus.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIWebBrowserPersist.h"
#include "nsIProperties.h"
//#include "nsIRequest.h"
//#include "nsIPrefService.h"
#include "nsISHistory.h"
#include "nsIHistoryEntry.h"
#include "nsISHEntry.h"
#include "nsNetUtil.h"
#include "SaveHeaderSniffer.h"
#include "nsIWebBrowserFind.h"

#import "CHBrowserView.h"

#import "CHBrowserService.h"
#import "CHBrowserListener.h"

#import "mozView.h"

typedef unsigned int DragReference;
#include "nsIDragHelperService.h"

// Cut/copy/paste
#include "nsIClipboardCommands.h"
#include "nsIInterfaceRequestorUtils.h"

// Undo/redo
#include "nsICommandManager.h"
#include "nsICommandParams.h"


const char kPersistContractID[] = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
const char kDirServiceContractID[] = "@mozilla.org/file/directory_service;1";

@implementation CHBrowserView

- (id)initWithFrame:(NSRect)frame andWindow:(NSWindow*)aWindow
{
  mWindow = aWindow;
  return [self initWithFrame:frame];
}

- (id)initWithFrame:(NSRect)frame
{
  if ( (self = [super initWithFrame:frame]) )
  {
    nsresult rv = CHBrowserService::InitEmbedding();
    if (NS_FAILED(rv)) {
// XXX need to throw
    }

    _listener = new CHBrowserListener(self);
    NS_ADDREF(_listener);
    
// Create the web browser instance
    nsCOMPtr<nsIWebBrowser> browser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
// XXX need to throw
    }

    _webBrowser = browser;
    NS_ADDREF(_webBrowser);
    
// Set the container nsIWebBrowserChrome
    _webBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, _listener));
    
// Register as a listener for web progress
    nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, _listener));
    _webBrowser->AddWebBrowserListener(weak, NS_GET_IID(nsIWebProgressListener));
    
// Hook up the widget hierarchy with us as the parent
    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(_webBrowser);
    baseWin->InitWindow((NSView*)self, nsnull, 0, 0,
                        frame.size.width, frame.size.height);
    baseWin->Create();
    
// register the view as a drop site for text, files, and urls. 
    [self registerForDraggedTypes: [NSArray arrayWithObjects:
              @"MozURLType", NSStringPboardType, NSURLPboardType, NSFilenamesPboardType, nil]];

    // hookup the listener for creating our own native menus on <SELECTS>
    CHClickListener* clickListener = new CHClickListener();
    if (!clickListener)
      return nil;
    
    nsCOMPtr<nsIDOMWindow> contentWindow = getter_AddRefs([self getContentWindow]);
    nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(contentWindow));
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(piWindow->GetChromeEventHandler()));
    if ( rec )
      rec->AddEventListenerByIID(clickListener, NS_GET_IID(nsIDOMMouseListener));
  }
  return self;
}

- (void)destroyWebBrowser
{
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(_webBrowser);
  if ( baseWin ) {
    // clean up here rather than in the dtor so that if anyone tries to get our
    // web browser, it won't get a garbage object that's past its prime. As a result,
    // this routine MUST be called otherwise we will leak.
    baseWin->Destroy();
    NS_RELEASE(_webBrowser);
  }
}

- (void)dealloc 
{
  NS_RELEASE(_listener);
  
  // it is imperative that |destroyWebBrowser()| be called before we get here, otherwise
  // we will leak the webBrowser.
	NS_ASSERTION(!_webBrowser, "BrowserView going away, destroyWebBrowser not called; leaking webBrowser!");
  
  CHBrowserService::BrowserClosed();
  
#if DEBUG
  NSLog(@"CHBrowserView died.");
#endif

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

- (void)addListener:(id <CHBrowserListener>)listener
{
  _listener->AddListener(listener);
}

- (void)removeListener:(id <CHBrowserListener>)listener
{
  _listener->RemoveListener(listener);
}

- (void)setContainer:(id <CHBrowserContainer>)container
{
  _listener->SetContainer(container);
}

- (nsIDOMWindow*)getContentWindow
{
  nsIDOMWindow* window = nsnull;

  if ( _webBrowser )
    _webBrowser->GetContentDOMWindow(&window);

  return window;
}

- (void)loadURI:(NSString *)urlSpec referrer:(NSString*)referrer flags:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  
  int length = [urlSpec length];
  PRUnichar* specStr = nsMemory::Alloc((length+1) * sizeof(PRUnichar));
  [urlSpec getCharacters:specStr];
  specStr[length] = PRUnichar(0);
  
  nsCOMPtr<nsIURI> referrerURI;
  if ( referrer )
    NS_NewURI(getter_AddRefs(referrerURI), [referrer UTF8String]);

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

  nsresult rv = nav->LoadURI(specStr, navFlags, referrerURI, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }

  nsMemory::Free(specStr);
}

- (void)reload:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return;

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
  if ( !nav )
    return NO;

  PRBool can;
  nav->GetCanGoBack(&can);

  return can ? YES : NO;
}

- (BOOL)canGoForward
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return NO;

  PRBool can;
  nav->GetCanGoForward(&can);

  return can ? YES : NO;
}

- (void)goBack
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return;

  nsresult rv = nav->GoBack();
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }  
}

- (void)goForward
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return;

  nsresult rv = nav->GoForward();
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }  
}

- (void)gotoIndex:(int)index
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return;

  nsresult rv = nav->GotoIndex(index);
  if (NS_FAILED(rv)) {
    // XXX need to throw
  }    
}

- (void)stop:(unsigned int)flags
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(_webBrowser);
  if ( !nav )
    return;

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
  if ( !nav )
    return nsnull;

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

- (CHBrowserListener*)getCocoaBrowserListener
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
  NS_IF_ADDREF(browser);				// prevents destroying |browser| if |browser| == |_webBrowser|
  NS_IF_RELEASE(_webBrowser);
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
    nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(kPersistContractID));
    if (!webPersist)
        return;
    
    // Make a temporary file object that we can save to.
    nsCOMPtr<nsIProperties> dirService(do_GetService(kDirServiceContractID));
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
      if (webNav) {
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
    }

    // when saving, we first fire off a save with a nsHeaderSniffer as a progress
    // listener. This allows us to look for the content-disposition header, which
    // can supply a filename, and maybe has something to do with CGI-generated
    // content (?)
    nsCAutoString fileName(aFilename);
    nsHeaderSniffer* sniffer = new nsHeaderSniffer(webPersist, tmpFile, aURI, 
                                                   aDocument, postData, fileName, aBypassCache,
                                                   aFilterView, aFilterList);
    if (!sniffer)
        return;
    webPersist->SetProgressListener(sniffer);  // owned
    webPersist->SaveURI(aURI, nsnull, nsnull, nsnull, nsnull, tmpFile);
}

-(void)printDocument
{
  if (!_webBrowser)
    return;
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
    if (!_webBrowser)
      return NO;
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
  nsresult rv = NS_NewURI(getter_AddRefs(url), [aURLSpec UTF8String]);
  if (NS_FAILED(rv))
    return;
  
  [self saveInternal: url.get()
        withDocument: nsnull
   suggestedFilename: (([aFilename length] > 0) ? [aFilename fileSystemRepresentation] : "")
         bypassCache: YES
          filterView: aFilterView
          filterList: aFilterList];
}

-(NSString*)getFocusedURLString
{
  if (!_webBrowser)
    return @"";
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
  return [NSString stringWith_nsAString: urlStr];
}

- (void)saveDocument: (NSView*)aFilterView filterList: (NSPopUpButton*)aFilterList
{
    if (!_webBrowser)
      return;
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
#warning fix me
    nsCOMPtr<nsIURI> url;
    nsresult rv = NS_NewURI(getter_AddRefs(url), urlStr);
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
    nsresult rv = commandMgr->DoCommand(commandName, nsnull, nsnull);
#if DEBUG
      if (NS_FAILED(rv))
        NSLog(@"DoCommand failed");
#endif
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
    nsresult rv = commandMgr->IsCommandEnabled(commandName, nsnull, &isEnabled);
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
  if ( clipboard )
    clipboard->CutSelection();
}

-(BOOL)canCut
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  if ( clipboard )
    clipboard->CanCutSelection(&canCut);
  return canCut;
}

-(IBAction)copy:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  if ( clipboard )
    clipboard->CopySelection();
}

-(BOOL)canCopy
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  if ( clipboard )
    clipboard->CanCopySelection(&canCut);
  return canCut;
}

-(IBAction)paste:(id)aSender
{
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  if ( clipboard )
    clipboard->Paste();
}

-(BOOL)canPaste
{
  PRBool canCut = PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(_webBrowser));
  if ( clipboard )
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
  if ( clipboard )
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
  if (!_webBrowser)
    return empty;
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
  return [NSString stringWith_nsAString: urlStr];
}

- (void)setActive: (BOOL)aIsActive
{
  nsCOMPtr<nsIWebBrowserFocus> wbf(do_QueryInterface(_webBrowser));
  if (wbf) {
    if (aIsActive)
      wbf->Activate();
    else
      wbf->Deactivate();
  }
}

-(NSMenu*)getContextMenu
{
  if ([[self superview] conformsToProtocol:@protocol(CHBrowserContainer)])
  {
    id<CHBrowserContainer> browserContainer = [self superview];
  	return [browserContainer getContextMenu];
  }
  
  return nil;
}

-(NSWindow*)getNativeWindow
{
  NSWindow* window = [self window];
  if (window)
    return window; // We're visible.  Just hand the window back.

  // We're invisible.  It's likely that we're in a Cocoa tab view.
  // First see if we have a cached window.
  if (mWindow)
    return mWindow;
  
  // Finally, see if our parent responds to the getNativeWindow selector,
  // and if they do, let them handle it.
  if ([[self superview] conformsToProtocol:@protocol(CHBrowserContainer)])
  {
    id<CHBrowserContainer> browserContainer = [self superview];
    return [browserContainer getNativeWindow];
  }
    
  return nil;
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

#pragma mark -

- (BOOL)shouldAcceptDrag:(id <NSDraggingInfo>)sender
{
  if ([[self superview] conformsToProtocol:@protocol(CHBrowserContainer)])
  {
    id<CHBrowserContainer> browserContainer = [self superview];
    return [browserContainer shouldAcceptDragFromSource:[sender draggingSource]];
  }
  return YES;
}

- (unsigned int)draggingEntered:(id <NSDraggingInfo>)sender
{
  if (![self shouldAcceptDrag:sender])
    return NSDragOperationNone;

//  NSLog(@"draggingEntered");  
  nsCOMPtr<nsIDragHelperService> helper(do_GetService("@mozilla.org/widget/draghelperservice;1"));
  mDragHelper = helper.get();
  NS_IF_ADDREF(mDragHelper);
  NS_ASSERTION ( mDragHelper, "Couldn't get a drag service, we're in big trouble" );
  
  if ( mDragHelper ) {
    mLastTrackedLocation = [sender draggingLocation];
    mLastTrackedWindow   = [sender draggingDestinationWindow];
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:mLastTrackedLocation inWindow:mLastTrackedWindow];
    if (sink)
      mDragHelper->Enter ( [sender draggingSequenceNumber], sink );
  }
  
  return NSDragOperationCopy;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
//  NSLog(@"draggingExited");
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    
    [self findEventSink:getter_AddRefs(sink)
            forPoint:mLastTrackedLocation /* [sender draggingLocation] */
            inWindow:mLastTrackedWindow   /* [sender draggingDestinationWindow] */
            ];
    if (sink)
      mDragHelper->Leave( [sender draggingSequenceNumber], sink );
    NS_RELEASE(mDragHelper);     
  }
}

- (unsigned int)draggingUpdated:(id <NSDraggingInfo>)sender
{
  if (![self shouldAcceptDrag:sender])
    return NSDragOperationNone;

//  NSLog(@"draggingUpdated");
  PRBool dropAllowed = PR_FALSE;
  if ( mDragHelper ) {
    mLastTrackedLocation = [sender draggingLocation];
    mLastTrackedWindow   = [sender draggingDestinationWindow];
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:mLastTrackedLocation inWindow:mLastTrackedWindow];
    if (sink)
      mDragHelper->Tracking([sender draggingSequenceNumber], sink, &dropAllowed);
  }
  
  return dropAllowed ? NSDragOperationCopy : NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
  return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  if (![self shouldAcceptDrag:sender])
    return NO;

  PRBool dragAccepted = PR_FALSE;
    
  if ( mDragHelper ) {
    nsCOMPtr<nsIEventSink> sink;
    [self findEventSink:getter_AddRefs(sink) forPoint:[sender draggingLocation]
            inWindow:[sender draggingDestinationWindow]];
    if (sink)
      mDragHelper->Drop([sender draggingSequenceNumber], sink, &dragAccepted);
  }
  
  return dragAccepted ? YES : NO;
}

#pragma mark -

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
