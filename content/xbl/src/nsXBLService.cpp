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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *  - Brendan Eich (brendan@mozilla.org)
 *  - Mike Pinkerton (pinkerton@netscape.com)
 *
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

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsXBLService.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsXBLWindowDragHandler.h"
#include "nsIInputStream.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIDOMElement.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsHTMLAtoms.h"
#include "nsSupportsArray.h"
#include "nsITextContent.h"
#include "nsIMemory.h"
#include "nsIObserverService.h"
#include "nsIDOMNodeList.h"
#include "nsXBLContentSink.h"
#include "nsIXBLBinding.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsXBLAtoms.h"

#include "nsIXBLPrototypeHandler.h"

#include "nsIChromeRegistry.h"
#include "nsIPref.h"

#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"

#include "nsIXULPrototypeCache.h"
#include "nsIDOMLoadListener.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kParserCID,                  NS_PARSER_CID);

static PRBool IsChromeOrResourceURI(nsIURI* aURI)
{
  PRBool isChrome = PR_FALSE;
  PRBool isResource = PR_FALSE;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && 
      NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)))
      return (isChrome || isResource);
  return PR_FALSE;
}

static PRBool IsResourceURI(nsIURI* aURI)
{
  PRBool isResource = PR_FALSE;
  if (NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)) && isResource)
      return PR_TRUE; 
  return PR_FALSE;
}

// Individual binding requests.
class nsXBLBindingRequest
{
public:
  nsCString mBindingURL;
  nsCOMPtr<nsIContent> mBoundElement;

  static nsXBLBindingRequest*
  Create(nsFixedSizeAllocator& aPool, const nsCString& aURL, nsIContent* aBoundElement) {
    void* place = aPool.Alloc(sizeof(nsXBLBindingRequest));
    return place ? ::new (place) nsXBLBindingRequest(aURL, aBoundElement) : nsnull;
  }

  static void
  Destroy(nsFixedSizeAllocator& aPool, nsXBLBindingRequest* aRequest) {
    aRequest->~nsXBLBindingRequest();
    aPool.Free(aRequest, sizeof(*aRequest));
  }

  void DocumentLoaded(nsIDocument* aBindingDoc)
  {
    nsCOMPtr<nsIDocument> doc;
    mBoundElement->GetDocument(*getter_AddRefs(doc));
    if (!doc)
      return;

    // Get the binding.
    PRBool ready = PR_FALSE;
    gXBLService->BindingReady(mBoundElement, mBindingURL, &ready);

    if (!ready)
      return;

    // XXX Deal with layered bindings.
    // Now do a ContentInserted notification to cause the frames to get installed finally,
    nsCOMPtr<nsIContent> parent;
    mBoundElement->GetParent(*getter_AddRefs(parent));
    PRInt32 index = 0;
    if (parent)
      parent->IndexOf(mBoundElement, index);
        
    nsCOMPtr<nsIPresShell> shell;
    doc->GetShellAt(0, getter_AddRefs(shell));
    if (shell) {
      nsIFrame* childFrame;
      shell->GetPrimaryFrameFor(mBoundElement, &childFrame);
      nsCOMPtr<nsIDocumentObserver> obs(do_QueryInterface(shell));
      if (!childFrame)
        obs->ContentInserted(doc, parent, mBoundElement, index);
    }
  }

  static nsIXBLService* gXBLService;
  static int gRefCnt;

protected:
  nsXBLBindingRequest(const nsCString& aURL, nsIContent* aBoundElement)
  {
    mBindingURL = aURL;
    mBoundElement = aBoundElement;

    gRefCnt++;
    if (gRefCnt == 1) {
      nsServiceManager::GetService("@mozilla.org/xbl;1",
                                   NS_GET_IID(nsIXBLService),
                                   (nsISupports**) &gXBLService);
    }
  }

  ~nsXBLBindingRequest()
  {
    gRefCnt--;
    if (gRefCnt == 0) {
      nsServiceManager::ReleaseService("@mozilla.org/xbl;1", gXBLService);
      gXBLService = nsnull;
    }
  }

private:
  // Hide so that only Create() and Destroy() can be used to
  // allocate and deallocate from the heap
  static void* operator new(size_t) { return 0; }
  static void operator delete(void*, size_t) {}
};

static const size_t kBucketSizes[] = {
  sizeof(nsXBLBindingRequest)
};

static const PRInt32 kNumBuckets = sizeof(kBucketSizes)/sizeof(size_t);
static const PRInt32 kNumElements = 64;
static const PRInt32 kInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLBindingRequest))) * kNumElements;

nsIXBLService* nsXBLBindingRequest::gXBLService = nsnull;
int nsXBLBindingRequest::gRefCnt = 0;

// nsXBLStreamListener, a helper class used for 
// asynchronous parsing of URLs
/* Header file */
class nsXBLStreamListener : public nsIStreamListener, public nsIDOMLoadListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD Abort(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD Error(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };

  static nsIXULPrototypeCache* gXULCache;
  static PRInt32 gRefCnt;

  nsXBLStreamListener(nsXBLService* aXBLService, nsIStreamListener* aInner, nsIDocument* aDocument, nsIDocument* aBindingDocument);
  virtual ~nsXBLStreamListener();
  
  void AddRequest(nsXBLBindingRequest* aRequest) { mBindingRequests.AppendElement(aRequest); };
  PRBool HasRequest(const nsCString& aURI, nsIContent* aBoundElement);

private:
  nsXBLService* mXBLService; // [WEAK]

  nsCOMPtr<nsIStreamListener> mInner;
  nsAutoVoidArray mBindingRequests;
  
  nsCOMPtr<nsIWeakReference> mDocument;
  nsCOMPtr<nsIDocument> mBindingDocument;
};

nsIXULPrototypeCache* nsXBLStreamListener::gXULCache = nsnull;
PRInt32 nsXBLStreamListener::gRefCnt = 0;

/* Implementation file */
NS_IMPL_ISUPPORTS4(nsXBLStreamListener, nsIStreamListener, nsIRequestObserver, nsIDOMLoadListener, nsIDOMEventListener)

nsXBLStreamListener::nsXBLStreamListener(nsXBLService* aXBLService,
                                         nsIStreamListener* aInner, nsIDocument* aDocument,
                                         nsIDocument* aBindingDocument)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mXBLService = aXBLService;
  mInner = aInner;
  mDocument = getter_AddRefs(NS_GetWeakReference(aDocument));
  mBindingDocument = aBindingDocument;
  gRefCnt++;
  if (gRefCnt == 1) {
    nsresult rv;
    rv = nsServiceManager::GetService("@mozilla.org/xul/xul-prototype-cache;1",
                                      NS_GET_IID(nsIXULPrototypeCache),
                                      (nsISupports**) &gXULCache);
    if (NS_FAILED(rv)) return;
  }
}

nsXBLStreamListener::~nsXBLStreamListener()
{
  /* destructor code */
  gRefCnt--;
  if (gRefCnt == 0) {
    if (gXULCache) {
      nsServiceManager::ReleaseService("@mozilla.org/xul/xul-prototype-cache;1", gXULCache);
      gXULCache = nsnull;
    }
  }
}

NS_IMETHODIMP
nsXBLStreamListener::OnDataAvailable(nsIRequest *request, nsISupports* aCtxt, nsIInputStream* aInStr, 
                                     PRUint32 aSourceOffset, PRUint32 aCount)
{
  if (mInner)
    return mInner->OnDataAvailable(request, aCtxt, aInStr, aSourceOffset, aCount);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXBLStreamListener::OnStartRequest(nsIRequest* request, nsISupports* aCtxt)
{
  if (mInner)
    return mInner->OnStartRequest(request, aCtxt);
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsXBLStreamListener::OnStopRequest(nsIRequest* request, nsISupports* aCtxt, nsresult aStatus)
{
  nsresult rv = NS_OK;
  if (mInner) {
     rv = mInner->OnStopRequest(request, aCtxt, aStatus);
  }

  if (NS_FAILED(rv) || NS_FAILED(aStatus))
  {
    nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
    if (aChannel)
  	{
      nsCOMPtr<nsIURI> channelURI;
      aChannel->GetURI(getter_AddRefs(channelURI));
      nsXPIDLCString str;
      channelURI->GetSpec(getter_Copies(str));
      printf("Failed to load XBL document %s\n", (const char*)str);
  	}

    PRUint32 count = mBindingRequests.Count();
    for (PRUint32 i = 0; i < count; i++) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
      nsXBLBindingRequest::Destroy(mXBLService->mPool, req);
    }

    mBindingRequests.Clear();
    mDocument = nsnull;
    mBindingDocument = nsnull;
  }

  return rv;
}

PRBool
nsXBLStreamListener::HasRequest(const nsCString& aURI, nsIContent* aElt)
{
  // XXX Could be more efficient.
  PRUint32 count = mBindingRequests.Count();
  for (PRUint32 i = 0; i < count; i++) {
    nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
    if (req->mBindingURL.Equals(aURI) && req->mBoundElement.get() == aElt)
      return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
nsXBLStreamListener::Load(nsIDOMEvent* aEvent)
{
  nsresult rv = NS_OK;
  PRUint32 i;
  PRUint32 count = mBindingRequests.Count();
  
  // See if we're still alive.
  nsCOMPtr<nsIDocument> doc(do_QueryReferent(mDocument));
  if (!doc) {
    NS_WARNING("XBL load did not complete until after document went away! Modal dialog bug?\n");
  }
  else {
    // We have to do a flush prior to notification of the document load.
    // This has to happen since the HTML content sink can be holding on
    // to notifications related to our children (e.g., if you bind to the
    // <body> tag) that result in duplication of content.  
    // We need to get the sink's notifications flushed and then make the binding
    // ready.
    if (count > 0) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(0);
      nsCOMPtr<nsIDocument> document;
      req->mBoundElement->GetDocument(*getter_AddRefs(document));
      if (document)
        document->FlushPendingNotifications();
    }

    // Remove ourselves from the set of pending docs.
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIURI> uri;
    mBindingDocument->GetDocumentURL(getter_AddRefs(uri));
    nsXPIDLCString str;
    uri->GetSpec(getter_Copies(str));
    bindingManager->RemoveLoadingDocListener(nsCAutoString(NS_STATIC_CAST(const char*, str)));

    nsCOMPtr<nsIContent> root;
    mBindingDocument->GetRootContent(getter_AddRefs(root));
    if (!root) {
      NS_ERROR("*** XBL doc with no root element! Something went horribly wrong! ***");
      return NS_ERROR_FAILURE;
    }

    // Put our doc info in the doc table.
    nsCOMPtr<nsIXBLDocumentInfo> info;
    nsCOMPtr<nsIBindingManager> xblDocBindingManager;
    mBindingDocument->GetBindingManager(getter_AddRefs(xblDocBindingManager));
    xblDocBindingManager->GetXBLDocumentInfo(nsCAutoString(NS_STATIC_CAST(const char*, str)), getter_AddRefs(info));
    xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.
    if (!info) {
      NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
      return NS_ERROR_FAILURE;
    }

    // If the doc is a chrome URI, then we put it into the XUL cache.
    PRBool cached = PR_FALSE;
    if (IsChromeOrResourceURI(uri)) {
      PRBool useXULCache;
      gXULCache->GetEnabled(&useXULCache);
      if (useXULCache) {
        cached = PR_TRUE;
        gXULCache->PutXBLDocumentInfo(info);
      }
    }
  
    if (!cached)
      bindingManager->PutXBLDocumentInfo(info);

    // Notify all pending requests that their bindings are
    // ready and can be installed.
    for (i = 0; i < count; i++) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
      req->DocumentLoaded(mBindingDocument);
    }

    // All reqs normally have the same binding doc.  Force a synchronous
    // reflow on this binding doc to deal with the fact that iframes
    // don't construct or load their subdocs until they get a reflow.
    if (count > 0) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(0);
      nsCOMPtr<nsIDocument> document;
      req->mBoundElement->GetDocument(*getter_AddRefs(document));
      if (document)
        document->FlushPendingNotifications();
    }
  }
  
  for (i = 0; i < count; i++) {
    nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
    nsXBLBindingRequest::Destroy(mXBLService->mPool, req);
  }

  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(mBindingDocument));
  rec->RemoveEventListener(NS_LITERAL_STRING("load"), (nsIDOMLoadListener*)this, PR_FALSE);

  mBindingRequests.Clear();
  mDocument = nsnull;
  mBindingDocument = nsnull;

  return rv;
}

// nsProxyStream 
// A helper class used for synchronous parsing of URLs.
class nsProxyStream : public nsIInputStream
{
private:
  const char* mBuffer;
  PRUint32    mSize;
  PRUint32    mIndex;

public:
  nsProxyStream(void) : mBuffer(nsnull)
  {
      NS_INIT_REFCNT();
  }

  virtual ~nsProxyStream(void) {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseStream
  NS_IMETHOD Close(void) {
      return NS_OK;
  }

  // nsIInputStream
  NS_IMETHOD Available(PRUint32 *aLength) {
      *aLength = mSize - mIndex;
      return NS_OK;
  }

  NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
      PRUint32 readCount = PR_MIN(aCount, (mSize-mIndex));
      
      nsCRT::memcpy(aBuf, mBuffer+mIndex, readCount);
      mIndex += readCount;

      *aReadCount = readCount;

      return NS_OK;
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
    PRUint32 readCount = PR_MIN(count, (mSize-mIndex));
    
    *_retval = 0;
    nsresult rv = writer (this, closure, mBuffer+mIndex, mIndex, readCount, _retval);
    mIndex += *_retval;
    
    return rv;
  }

  NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Implementation
  void SetBuffer(const char* aBuffer, PRUint32 aSize) {
      mBuffer = aBuffer;
      mSize = aSize;
      mIndex = 0;
  }
};

NS_IMPL_ISUPPORTS1(nsProxyStream, nsIInputStream)


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
nsIXULPrototypeCache* nsXBLService::gXULCache = nsnull;
 
nsINameSpaceManager* nsXBLService::gNameSpaceManager = nsnull;
 
nsHashtable* nsXBLService::gClassTable = nsnull;

JSCList  nsXBLService::gClassLRUList = JS_INIT_STATIC_CLIST(&nsXBLService::gClassLRUList);
PRUint32 nsXBLService::gClassLRUListLength = 0;
PRUint32 nsXBLService::gClassLRUListQuota = 64;

nsIAtom* nsXBLService::kEventAtom = nsnull;
nsIAtom* nsXBLService::kScrollbarAtom = nsnull;
nsIAtom* nsXBLService::kInputAtom = nsnull;

// Enabled by default. Must be over-ridden to disable
PRBool nsXBLService::gDisableChromeCache = PR_FALSE;
static const char kDisableChromeCachePref[] = "nglayout.debug.disable_xul_cache";

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsXBLService, nsIXBLService, nsIObserver, nsISupportsWeakReference)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  NS_INIT_REFCNT();
  mPool.Init("XBL Binding Requests", kBucketSizes, kNumBuckets, kInitialSize);

  gRefCnt++;
  if (gRefCnt == 1) {
    nsresult rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                     nsnull,
                                                     NS_GET_IID(nsINameSpaceManager),
                                                     (void**) &gNameSpaceManager);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
    if (NS_FAILED(rv)) return;
    
    // Create our atoms
    kEventAtom = NS_NewAtom("event");
    kScrollbarAtom = NS_NewAtom("scrollbar");
    kInputAtom = NS_NewAtom("input");

    // Find out if the XUL cache is on or off
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
      prefs->GetBoolPref(kDisableChromeCachePref, &gDisableChromeCache);

    gClassTable = new nsHashtable();

    rv = nsServiceManager::GetService("@mozilla.org/xul/xul-prototype-cache;1",
                                      NS_GET_IID(nsIXULPrototypeCache),
                                      (nsISupports**) &gXULCache);
    if (NS_FAILED(rv)) return;
  }
}

nsXBLService::~nsXBLService(void)
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_IF_RELEASE(gNameSpaceManager);
    
    // Release our atoms
    NS_RELEASE(kEventAtom);
    NS_RELEASE(kScrollbarAtom);
    NS_RELEASE(kInputAtom);

    // Walk the LRU list removing and deleting the nsXBLJSClasses.
    FlushMemory();

    // Any straggling nsXBLJSClass instances held by unfinalized JS objects
    // created for bindings will be deleted when those objects are finalized
    // (and not put on gClassLRUList, because length >= quota).
    gClassLRUListLength = gClassLRUListQuota = 0;

    // At this point, the only hash table entries should be for referenced
    // XBL class structs held by unfinalized JS binding objects.
    delete gClassTable;
    gClassTable = nsnull;

    if (gXULCache) {
      nsServiceManager::ReleaseService("@mozilla.org/xul/xul-prototype-cache;1", gXULCache);
      gXULCache = nsnull;
    }
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, const nsAReadableString& aURL, PRBool aAugmentFlag,
                           nsIXBLBinding** aBinding, PRBool* aResolveStyle) 
{ 
  *aBinding = nsnull;
  *aResolveStyle = PR_FALSE;

  nsresult rv;

  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));

  // XXX document may be null if we're in the midst of paint suppression
  if (!document)
    return NS_OK;

  nsCOMPtr<nsIBindingManager> bindingManager;
  document->GetBindingManager(getter_AddRefs(bindingManager));
  
  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(aContent, getter_AddRefs(binding));
  if (binding && !aAugmentFlag) {
    nsCOMPtr<nsIXBLBinding> styleBinding;
    binding->GetFirstStyleBinding(getter_AddRefs(styleBinding));
    if (styleBinding) {
      PRBool marked = PR_FALSE;
      binding->MarkedForDeath(&marked);
      if (marked) {
        FlushStyleBindings(aContent);
        binding = nsnull;
      }
      else {
        // See if the URIs match.
        nsCAutoString uri;
        styleBinding->GetBindingURI(uri);
        if (uri.EqualsWithConversion((const PRUnichar *) PromiseFlatString(aURL).get()))
          return NS_OK;
        else {
          FlushStyleBindings(aContent);
          binding = nsnull;
        }
      }
    }
  }

  nsCOMPtr<nsIXBLBinding> newBinding;
  nsCAutoString url; url.AssignWithConversion(aURL);
  if (NS_FAILED(rv = GetBinding(aContent, url, getter_AddRefs(newBinding)))) {
    return rv;
  }

  if (!newBinding) {
    nsCAutoString str( "Failed to locate XBL binding. XBL is now using id instead of name to reference bindings. Make sure you have switched over.  The invalid binding name is: ");
    str.AppendWithConversion(aURL);
    NS_ERROR(str.get());
    return NS_OK;
  }

  if (aAugmentFlag) {
    nsCOMPtr<nsIXBLBinding> baseBinding;
    nsCOMPtr<nsIXBLBinding> nextBinding = newBinding;
    do {
      baseBinding = nextBinding;
      baseBinding->GetBaseBinding(getter_AddRefs(nextBinding));
      baseBinding->SetIsStyleBinding(PR_FALSE);
    } while (nextBinding);

    // XXX Handle adjusting the prototype chain! We need to somehow indicate to
    // InstallImplementation that the whole chain should just be whacked and rebuilt.
    // We are becoming the new binding.
    bindingManager->SetBinding(aContent, newBinding);
    baseBinding->SetBaseBinding(binding);
  }
  else {
    // We loaded a style binding.  It goes on the end.
    if (binding) {
      // Get the last binding that is in the append layer.
      nsCOMPtr<nsIXBLBinding> rootBinding;
      binding->GetRootBinding(getter_AddRefs(rootBinding));
      rootBinding->SetBaseBinding(newBinding);
    }
    else {
      // Install the binding on the content node.
      bindingManager->SetBinding(aContent, newBinding);
    }
  }

  // Set the binding's bound element.
  newBinding->SetBoundElement(aContent);

  // Tell the binding to build the anonymous content.
  newBinding->GenerateAnonymousContent();

  // Tell the binding to install event handlers
  newBinding->InstallEventHandlers();

  // Set up our properties
  newBinding->InstallImplementation();

  // Figure out if we need to execute a constructor.
  newBinding->GetFirstBindingWithConstructor(aBinding);

  // Figure out if we have any scoped sheets.  If so, we do a second resolve.
  newBinding->HasStyleSheets(aResolveStyle);
  
  return NS_OK; 
}

NS_IMETHODIMP
nsXBLService::FlushStyleBindings(nsIContent* aContent)
{
  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));

  // XXX doc will be null if we're in the midst of paint suppression.
  if (! document)
    return NS_OK;

  nsCOMPtr<nsIBindingManager> bindingManager;
  document->GetBindingManager(getter_AddRefs(bindingManager));
  
  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(aContent, getter_AddRefs(binding));
  
  if (binding) {
    nsCOMPtr<nsIXBLBinding> styleBinding;
    binding->GetFirstStyleBinding(getter_AddRefs(styleBinding));

    if (styleBinding) {
      // Clear out the script references.
      nsCOMPtr<nsIDocument> document;
      aContent->GetDocument(*getter_AddRefs(document));
      styleBinding->UnhookEventHandlers();
      styleBinding->ChangeDocument(document, nsnull);
    }

    if (styleBinding == binding) 
      bindingManager->SetBinding(aContent, nsnull); // Flush old style bindings
  }
   
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult)
{
  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    document->GetBindingManager(getter_AddRefs(bindingManager));
  
    if (bindingManager)
      return bindingManager->ResolveTag(aContent, aNameSpaceID, aResult);
  }

  aContent->GetNameSpaceID(*aNameSpaceID);
  aContent->GetTag(*aResult); // Addref happens here.
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::GetXBLDocumentInfo(const nsCString& aURLStr, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)
{
  *aResult = nsnull;

  PRBool useXULCache;
  gXULCache->GetEnabled(&useXULCache);
  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(aURLStr, aResult);
  }

  if (!*aResult) {
    // The second line of defense is the binding manager's document table.
    nsCOMPtr<nsIDocument> boundDocument;
    aBoundElement->GetDocument(*getter_AddRefs(boundDocument));
    if (boundDocument) {
      nsCOMPtr<nsIBindingManager> bindingManager;
      boundDocument->GetBindingManager(getter_AddRefs(bindingManager));
      bindingManager->GetXBLDocumentInfo(aURLStr, aResult);
    }
  }
  return NS_OK;
}


//
// AttachGlobalKeyHandler
//
// Creates a new key handler and prepares to listen to key events on the given
// event receiver (either a document or an content node). If the receiver is content,
// then extra work needs to be done to hook it up to the document (XXX WHY??)
//
NS_IMETHODIMP
nsXBLService::AttachGlobalKeyHandler(nsIDOMEventReceiver* aReceiver)
{
  // check if the receiver is a content node (not a document), and hook
  // it to the document if that is the case.
  nsCOMPtr<nsIDOMEventReceiver> rec = aReceiver;
  nsCOMPtr<nsIContent> contentNode(do_QueryInterface(aReceiver));
  if (contentNode) {
    nsCOMPtr<nsIDocument> doc;
    contentNode->GetDocument(*getter_AddRefs(doc));  
    if (doc)
      rec = do_QueryInterface(doc); // We're a XUL keyset. Attach to our document.
  }
    
  if (!rec)
    return NS_ERROR_FAILURE;
    
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(contentNode));

  // Create the key handler
  nsXBLWindowKeyHandler* handler;
  NS_NewXBLWindowKeyHandler(elt, rec, &handler); // This addRef's
  if (!handler)
    return NS_ERROR_FAILURE;

  // listen to these events
  rec->AddEventListener(NS_LITERAL_STRING("keydown"), handler, PR_FALSE);
  rec->AddEventListener(NS_LITERAL_STRING("keyup"), handler, PR_FALSE);
  rec->AddEventListener(NS_LITERAL_STRING("keypress"), handler, PR_FALSE);

  // Release.  Do this so that only the event receiver holds onto the key handler.
  NS_RELEASE(handler);

  return NS_OK;
}


//
// AttachGlobalDragDropHandler
//
// Creates a new drag handler and prepares to listen to dragNdrop events on the given
// event receiver.
//
NS_IMETHODIMP
nsXBLService::AttachGlobalDragHandler(nsIDOMEventReceiver* aReceiver)
{
  // Create the DnD handler
  nsXBLWindowDragHandler* handler;
  NS_NewXBLWindowDragHandler(aReceiver, &handler);
  if (!handler)
    return NS_ERROR_FAILURE;

  // listen to these events
  aReceiver->AddEventListener(NS_LITERAL_STRING("draggesture"), handler, PR_FALSE);
  aReceiver->AddEventListener(NS_LITERAL_STRING("dragenter"), handler, PR_FALSE);
  aReceiver->AddEventListener(NS_LITERAL_STRING("dragexit"), handler, PR_FALSE);
  aReceiver->AddEventListener(NS_LITERAL_STRING("dragover"), handler, PR_FALSE);
  aReceiver->AddEventListener(NS_LITERAL_STRING("dragdrop"), handler, PR_FALSE);

  // Release.  Do this so that only the event receiver holds onto the handler.
  NS_RELEASE(handler);

  return NS_OK;

} // AttachGlobalDragDropHandler


NS_IMETHODIMP
nsXBLService::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aSomeData)
{
  if (nsCRT::strcmp(aTopic, "memory-pressure") == 0)
    FlushMemory();

  return NS_OK;
}

nsresult
nsXBLService::FlushMemory()
{
  while (!JS_CLIST_IS_EMPTY(&gClassLRUList)) {
    JSCList* lru = gClassLRUList.next;
    nsXBLJSClass* c = NS_STATIC_CAST(nsXBLJSClass*, lru);

    JS_REMOVE_AND_INIT_LINK(lru);
    delete c;
    gClassLRUListLength--;
  }
  return NS_OK;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsXBLService::GetBinding(nsIContent* aBoundElement, 
                                       const nsCString& aURLStr, 
                                       nsIXBLBinding** aResult)
{
  PRBool dummy;
  return GetBindingInternal(aBoundElement, aURLStr, PR_FALSE, &dummy, aResult);
}

NS_IMETHODIMP nsXBLService::BindingReady(nsIContent* aBoundElement, 
                                         const nsCString& aURLStr, 
                                         PRBool* aIsReady)
{
  return GetBindingInternal(aBoundElement, aURLStr, PR_TRUE, aIsReady, nsnull);
}

NS_IMETHODIMP nsXBLService::GetBindingInternal(nsIContent* aBoundElement, 
                                               const nsCString& aURLStr, 
                                               PRBool aPeekOnly,
                                               PRBool* aIsReady, 
                                               nsIXBLBinding** aResult)
{
  if (aResult)
    *aResult = nsnull;

  if (aURLStr.IsEmpty())
    return NS_ERROR_FAILURE;

  // XXX Obtain the # marker and remove it from the URL.
  nsCAutoString uri(aURLStr);
  PRInt32 indx = uri.RFindChar('#');
  nsCAutoString ref; 
  uri.Right(ref, uri.Length() - (indx + 1));
  uri.Truncate(indx);

  nsCOMPtr<nsIDocument> boundDocument;
  aBoundElement->GetDocument(*getter_AddRefs(boundDocument));
    
  nsCOMPtr<nsIXBLDocumentInfo> docInfo;
  LoadBindingDocumentInfo(aBoundElement, boundDocument, uri, ref, PR_FALSE, getter_AddRefs(docInfo));
  if (!docInfo)
    return NS_ERROR_FAILURE;

  // Get our doc info and determine our script access.
  nsCOMPtr<nsIDocument> doc;
  docInfo->GetDocument(getter_AddRefs(doc));
  PRBool allowScripts;
  docInfo->GetScriptAccess(&allowScripts);

  nsCOMPtr<nsIXBLPrototypeBinding> protoBinding;
  docInfo->GetPrototypeBinding(ref, getter_AddRefs(protoBinding));

  NS_ASSERTION(protoBinding, "Unable to locate an XBL binding.");
  if (!protoBinding)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> child;
  protoBinding->GetBindingElement(getter_AddRefs(child));

  // Our prototype binding must have all its resources loaded.
  PRBool ready;
  protoBinding->LoadResources(&ready);
  if (!ready) {
    // Add our bound element to the protos list of elts that should
    // be notified when the stylesheets and scripts finish loading.
    protoBinding->AddResourceListener(aBoundElement);
    return NS_ERROR_FAILURE; // The binding isn't ready yet.
  }

  // If our prototype already has a base, then don't check for an "extends" attribute.
  nsCOMPtr<nsIXBLBinding> baseBinding;
  nsCOMPtr<nsIXBLPrototypeBinding> baseProto;
  PRBool hasBase;
  protoBinding->HasBasePrototype(&hasBase);
  protoBinding->GetBasePrototype(getter_AddRefs(baseProto));
  if (baseProto) {
    nsCAutoString url;
    baseProto->GetBindingURI(url);
    if (NS_FAILED(GetBindingInternal(aBoundElement, url, aPeekOnly, aIsReady, getter_AddRefs(baseBinding))))
      return NS_ERROR_FAILURE; // We aren't ready yet.
  }
  else if (hasBase) {
    // Check for the presence of 'extends' and 'display' attributes
    nsAutoString display, extends;
    child->GetAttr(kNameSpaceID_None, nsXBLAtoms::display, display);
    child->GetAttr(kNameSpaceID_None, nsXBLAtoms::extends, extends);
    PRBool hasDisplay = !display.IsEmpty();
    PRBool hasExtends = !extends.IsEmpty();
    
    nsAutoString value(extends);
         
    if (!hasExtends) 
      protoBinding->SetHasBasePrototype(PR_FALSE);
    else {
      // Now slice 'em up to see what we've got.
      nsAutoString prefix;
      PRInt32 offset;
      if (hasDisplay) {
        offset = display.FindChar(':');
        if (-1 != offset) {
          display.Left(prefix, offset);
          display.Cut(0, offset+1);
        }
      }
      else if (hasExtends) {
        offset = extends.FindChar(':');
        if (-1 != offset) {
          extends.Left(prefix, offset);
          extends.Cut(0, offset+1);
          display = extends;
        }
      }

      nsAutoString nameSpace;

      if (prefix.Length() > 0) {
        nsCOMPtr<nsIAtom> prefixAtom = getter_AddRefs(NS_NewAtom(prefix));

        nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(child));

        if (node) {
          node->LookupNamespaceURI(prefix, nameSpace);

          if (!nameSpace.IsEmpty()) {
            if (!hasDisplay) {
              // We extend some widget/frame. We don't really have a
              // base binding.
              protoBinding->SetHasBasePrototype(PR_FALSE);
              //child->UnsetAttr(kNameSpaceID_None, nsXBLAtoms::extends, PR_FALSE);
            }

            PRInt32 nameSpaceID;

            gNameSpaceManager->GetNameSpaceID(nameSpace, nameSpaceID);

            nsCOMPtr<nsIAtom> tagName = getter_AddRefs(NS_NewAtom(display));
            protoBinding->SetBaseTag(nameSpaceID, tagName);
          }
        }
      }

      if (hasExtends && (hasDisplay || nameSpace.IsEmpty())) {
        // Look up the prefix.
        // We have a base class binding. Load it right now.
        nsCAutoString urlCString; urlCString.AssignWithConversion(value);
        nsCOMPtr<nsIURI> docURI;
        doc->GetDocumentURL(getter_AddRefs(docURI));
        nsXPIDLCString urlStr;
        docURI->Resolve(urlCString.get(), getter_Copies(urlStr));
        urlCString = urlStr.get();
        if (NS_FAILED(GetBindingInternal(aBoundElement, urlCString, aPeekOnly, aIsReady, getter_AddRefs(baseBinding))))
          return NS_ERROR_FAILURE; // Binding not yet ready or an error occurred.
        if (!aPeekOnly) {
          // Make sure to set the base prototype.
          baseBinding->GetPrototypeBinding(getter_AddRefs(baseProto));
          protoBinding->SetBasePrototype(baseProto);
          child->UnsetAttr(kNameSpaceID_None, nsXBLAtoms::extends, PR_FALSE);
          child->UnsetAttr(kNameSpaceID_None, nsXBLAtoms::display, PR_FALSE);
        }
      }
    }
  }

  *aIsReady = PR_TRUE;
  if (!aPeekOnly) {
    // Make a new binding
    NS_NewXBLBinding(protoBinding, aResult);
    if (baseBinding)
      (*aResult)->SetBaseBinding(baseBinding);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                      const nsCString& aURLStr, const nsCString& aRef,
                                      PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult)
{
  nsresult rv = NS_OK;

  *aResult = nsnull;

  // We've got a file.  Check our XBL document cache.
  PRBool useXULCache;
  gXULCache->GetEnabled(&useXULCache);

  nsCOMPtr<nsIXBLDocumentInfo> info;
  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so that any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(aURLStr, getter_AddRefs(info));
  }

  if (!info) {
    // The second line of defense is the binding manager's document table.
    nsCOMPtr<nsIBindingManager> bindingManager;

    if (aBoundDocument) {
      aBoundDocument->GetBindingManager(getter_AddRefs(bindingManager));
      bindingManager->GetXBLDocumentInfo(aURLStr, getter_AddRefs(info));
    }

    nsCOMPtr<nsIAtom> tagName;
    if (aBoundElement)
      aBoundElement->GetTag(*getter_AddRefs(tagName));
    if (!info && bindingManager && (tagName.get() != kScrollbarAtom) && (tagName.get() != kInputAtom) 
        && !aForceSyncLoad) {
      // The third line of defense is to investigate whether or not the
      // document is currently being loaded asynchronously.  If so, there's no
      // document yet, but we need to glom on our request so that it will be
      // processed whenever the doc does finish loading.
      nsCOMPtr<nsIStreamListener> listener;
      if (bindingManager)
        bindingManager->GetLoadingDocListener(aURLStr, getter_AddRefs(listener));
      if (listener) {
        nsIStreamListener* ilist = listener.get();
        nsXBLStreamListener* xblListener = NS_STATIC_CAST(nsXBLStreamListener*, ilist);
        // Create a new load observer.
        nsCAutoString bindingURI(aURLStr);
        bindingURI += "#";
        bindingURI += aRef;
        if (!xblListener->HasRequest(bindingURI, aBoundElement)) {
          nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool, bindingURI, aBoundElement);
          xblListener->AddRequest(req);
        }
        return NS_OK;
      }
    }
     
    if (!info) {
      // Finally, if all lines of defense fail, we go and fetch the binding
      // document.
      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), aURLStr.get());
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a url");

      nsCOMPtr<nsIDocument> document;
      FetchBindingDocument(aBoundElement, aBoundDocument, uri, aRef, aForceSyncLoad, getter_AddRefs(document));
   
      if (document) {
        nsCOMPtr<nsIBindingManager> xblDocBindingManager;
        document->GetBindingManager(getter_AddRefs(xblDocBindingManager));
        xblDocBindingManager->GetXBLDocumentInfo(aURLStr, getter_AddRefs(info));
        xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.

        if (!info) {
          NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
          return NS_ERROR_FAILURE;
        }

        // If the doc is a chrome URI, then we put it into the XUL cache.
        PRBool cached = PR_FALSE;
        if (IsChromeOrResourceURI(uri)) {
          if (useXULCache) {
            cached = PR_TRUE;
            gXULCache->PutXBLDocumentInfo(info);
          }
        }
        
        if (!cached && bindingManager) {
          // Otherwise we put it in our binding manager's document table.
          bindingManager->PutXBLDocumentInfo(info);
        }
      }
    }
  }

  if (!info)
    return NS_OK;
 
  *aResult = info;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::FetchBindingDocument(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                   nsIURI* aURI, const nsCString& aRef, 
                                   PRBool aForceSyncLoad, nsIDocument** aResult)
{
  // Initialize our out pointer to nsnull
  *aResult = nsnull;

  // Create the XML document
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = nsComponentManager::CreateInstance(kXMLDocumentCID, nsnull,
                                                   NS_GET_IID(nsIDocument),
                                                   getter_AddRefs(doc));

  if (NS_FAILED(rv)) return rv;

  // XXX This is evil, but we're living in layout, so I'm
  // just going to do it.
  nsXMLDocument* xmlDoc = (nsXMLDocument*)(doc.get());

  // Now we have to synchronously load the binding file.
  // Create an XML content sink and a parser. 
  nsCOMPtr<nsILoadGroup> loadGroup;
  if (aBoundDocument)
    aBoundDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  nsCOMPtr<nsIRequest> request;
  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), aURI, nsnull, loadGroup);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIAtom> tagName;
  if (aBoundElement)
    aBoundElement->GetTag(*getter_AddRefs(tagName)); 

  if (tagName.get() == kScrollbarAtom || IsResourceURI(aURI))
    aForceSyncLoad = PR_TRUE;

  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIXMLContentSink> xblSink;
  NS_NewXBLContentSink(getter_AddRefs(xblSink), xmlDoc, aURI, nsnull);
  if (!xblSink)
    return NS_ERROR_FAILURE;

  if(!aForceSyncLoad) {
    if (NS_FAILED(rv = xmlDoc->StartDocumentLoad("loadAsData", 
                                                 channel, 
                                                 loadGroup, 
                                                 nsnull, 
                                                 getter_AddRefs(listener),
                                                 PR_TRUE,
                                                 xblSink))) {
      NS_ERROR("Failure to init XBL doc prior to load.");
      return rv;
    }
    // We can be asynchronous
    nsXBLStreamListener* xblListener = new nsXBLStreamListener(this, listener, aBoundDocument, doc);
    NS_ENSURE_TRUE(xblListener,NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(doc));
    rec->AddEventListener(NS_LITERAL_STRING("load"), (nsIDOMLoadListener*)xblListener, PR_FALSE);

    // Add ourselves to the list of loading docs.
    nsCOMPtr<nsIBindingManager> bindingManager;
    if (aBoundDocument)
      aBoundDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsXPIDLCString uri;
    aURI->GetSpec(getter_Copies(uri));
    if (bindingManager)
      bindingManager->PutLoadingDocListener(nsCAutoString(NS_STATIC_CAST(const char*, uri)), xblListener);

    // Add our request.
    nsCAutoString bindingURI(uri);
    bindingURI += "#";
    bindingURI += aRef;
    nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool, bindingURI, aBoundElement);
    xblListener->AddRequest(req);

    // Now kick off the async read.
    channel->AsyncOpen(xblListener, nsnull);
    return NS_OK;
  }

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open(getter_AddRefs(in));

  // If we couldn't open the channel, then just return.
  if (NS_FAILED(rv)) return NS_OK;

  request = do_QueryInterface(channel);

  NS_ASSERTION(request != nsnull, "no request info");
  NS_ASSERTION(in != nsnull, "no input stream");
  
  NS_ENSURE_TRUE(in,NS_ERROR_FAILURE);

  nsProxyStream* proxy = new nsProxyStream();
  NS_ENSURE_TRUE(proxy,NS_ERROR_OUT_OF_MEMORY);

  // Call StartDocumentLoad
  if (NS_FAILED(rv = xmlDoc->StartDocumentLoad("loadAsData", 
                                               channel, 
                                               loadGroup, 
                                               nsnull, 
                                               getter_AddRefs(listener),
                                               PR_TRUE,
                                               xblSink))) {
    NS_ERROR("Failure to init XBL doc prior to load.");
    return rv;
  }

  listener->OnStartRequest(request, nsnull);
  
  while (PR_TRUE) {
    char buf[1024];
    PRUint32 readCount;

    if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
        break; // error

    if (readCount == 0)
        break; // eof

    proxy->SetBuffer(buf, readCount);

    PRUint32 sourceOffset = 0;
    rv = listener->OnDataAvailable(request, nsnull, proxy, sourceOffset, readCount);
    sourceOffset += readCount;
    if (NS_FAILED(rv))
        break;
  }
  listener->OnStopRequest(request, nsnull, NS_OK);

  // don't leak proxy!
  proxy->Close();
  delete proxy;

  // The document is parsed. We now have a prototype document.
  // Everything worked, so we can just hand this back now.
  *aResult = doc;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

static void GetImmediateChild(nsIAtom* aTag, nsIContent* aParent, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aParent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aParent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult NS_NewXBLService(nsIXBLService** aResult);

nsresult
NS_NewXBLService(nsIXBLService** aResult)
{
  nsXBLService* result = new nsXBLService;
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = result);

  // Register the first (and only) nsXBLService as a memory pressure observer
  // so it can flush the LRU list in low-memory situations.
  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
  if (os)
    os->AddObserver(result, "memory-pressure", PR_TRUE);

  return NS_OK;
}

