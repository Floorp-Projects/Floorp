/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 */

#include "nsCOMPtr.h"
#include "nsXBLService.h"
#include "nsIInputStream.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIXMLContent.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsHTMLAtoms.h"
#include "nsSupportsArray.h"
#include "nsITextContent.h"

#include "nsIXBLBinding.h"
#include "nsIXBLDocumentInfo.h"

#include "nsIXBLPrototypeHandler.h"

#include "nsIChromeRegistry.h"
#include "nsIPref.h"

#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"

#include "nsIXULContentUtils.h"
#include "nsIXULPrototypeCache.h"
#include "nsIDOMLoadListener.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kParserCID,                  NS_PARSER_IID); // XXX What's up with this???
static NS_DEFINE_CID(kChromeRegistryCID,          NS_CHROMEREGISTRY_CID);

static PRBool IsChromeURI(nsIURI* aURI)
{
  nsresult rv;
  nsXPIDLCString protocol;
  rv = aURI->GetScheme(getter_Copies(protocol));
  if (NS_SUCCEEDED(rv)) {
    if (!PL_strcmp(protocol, "chrome")) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// Individual binding requests.
struct nsXBLBindingRequest
{
  nsCString mBindingURL;
  nsCOMPtr<nsIContent> mBoundElement;

  nsXBLBindingRequest(const nsCString& aURL, nsIContent* aBoundElement)
  {
    mBindingURL = aURL;
    mBoundElement = aBoundElement;

    gRefCnt++;
    if (gRefCnt == 1) {
      nsServiceManager::GetService("component://netscape/xbl",
                                   NS_GET_IID(nsIXBLService),
                                   (nsISupports**) &gXBLService);
    }
  }

  ~nsXBLBindingRequest()
  {
    gRefCnt--;
    if (gRefCnt == 0) {
      nsServiceManager::ReleaseService("component://netscape/xbl", gXBLService);
      gXBLService = nsnull;
    }
  }

  static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
    return aAllocator.Alloc(aSize);
  }

  static void operator delete(void* aPtr, size_t aSize) {
    nsFixedSizeAllocator::Free(aPtr, aSize);
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
        
    nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(0));
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
  NS_DECL_NSISTREAMOBSERVER

  nsresult Load(nsIDOMEvent* aEvent);
  nsresult Unload(nsIDOMEvent* aEvent) { return NS_OK; };
  nsresult Abort(nsIDOMEvent* aEvent) { return NS_OK; };
  nsresult Error(nsIDOMEvent* aEvent) { return NS_OK; };
  nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };

  static nsIXULPrototypeCache* gXULCache;
  static nsIXULContentUtils* gXULUtils;
  static PRInt32 gRefCnt;

  nsXBLStreamListener(nsIStreamListener* aInner, nsIDocument* aDocument, nsIDocument* aBindingDocument);
  virtual ~nsXBLStreamListener();
  
  void AddRequest(nsXBLBindingRequest* aRequest) { mBindingRequests.AppendElement(aRequest); };
  PRBool HasRequest(const nsCString& aURI, nsIContent* aBoundElement);

private:
  nsCOMPtr<nsIStreamListener> mInner;
  nsVoidArray mBindingRequests;
  
  nsCOMPtr<nsIWeakReference> mDocument;
  nsCOMPtr<nsIDocument> mBindingDocument;
};

nsIXULPrototypeCache* nsXBLStreamListener::gXULCache = nsnull;
nsIXULContentUtils* nsXBLStreamListener::gXULUtils = nsnull;
PRInt32 nsXBLStreamListener::gRefCnt = 0;

/* Implementation file */
NS_IMPL_ISUPPORTS4(nsXBLStreamListener, nsIStreamListener, nsIStreamObserver, nsIDOMLoadListener, nsIDOMEventListener)

nsXBLStreamListener::nsXBLStreamListener(nsIStreamListener* aInner, nsIDocument* aDocument,
                                         nsIDocument* aBindingDocument)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mInner = aInner;
  mDocument = getter_AddRefs(NS_GetWeakReference(aDocument));
  mBindingDocument = aBindingDocument;
  gRefCnt++;
  if (gRefCnt == 1) {
    nsresult rv = nsServiceManager::GetService("component://netscape/rdf/xul-content-utils",
                                      NS_GET_IID(nsIXULContentUtils),
                                      (nsISupports**) &gXULUtils);
    if (NS_FAILED(rv)) return;

    rv = nsServiceManager::GetService("component://netscape/rdf/xul-prototype-cache",
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
    if (gXULUtils) {
      nsServiceManager::ReleaseService("component://netscape/rdf/xul-content-utils", gXULUtils);
      gXULUtils = nsnull;
    }

    if (gXULCache) {
      nsServiceManager::ReleaseService("component://netscape/rdf/xul-prototype-cache", gXULCache);
      gXULCache = nsnull;
    }
  }
}

/* void onDataAvailable (in nsIChannel channel, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP
nsXBLStreamListener::OnDataAvailable(nsIChannel* aChannel, nsISupports* aCtxt, nsIInputStream* aInStr, 
                                     PRUint32 aSourceOffset, PRUint32 aCount)
{
  if (mInner)
    return mInner->OnDataAvailable(aChannel, aCtxt, aInStr, aSourceOffset, aCount);
  return NS_ERROR_FAILURE;
}

/* void onStartRequest (in nsIChannel channel, in nsISupports ctxt); */
NS_IMETHODIMP
nsXBLStreamListener::OnStartRequest(nsIChannel* aChannel, nsISupports* aCtxt)
{
  if (mInner)
    return mInner->OnStartRequest(aChannel, aCtxt);
    
  return NS_ERROR_FAILURE;
}

/* void onStopRequest (in nsIChannel channel, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP 
nsXBLStreamListener::OnStopRequest(nsIChannel* aChannel, nsISupports* aCtxt, nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv = NS_OK;
  if (mInner) {
     rv = mInner->OnStopRequest(aChannel, aCtxt, aStatus, aStatusArg);
  }

  if (NS_FAILED(rv) || NS_FAILED(aStatus)) {
    PRUint32 count = mBindingRequests.Count();
    for (PRUint32 i = 0; i < count; i++) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
      delete req;
    }

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
    // Remove ourselves from the set of pending docs.
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIURI> uri(getter_AddRefs(mBindingDocument->GetDocumentURL()));
    nsXPIDLCString str;
    uri->GetSpec(getter_Copies(str));
    bindingManager->RemoveLoadingDocListener(nsCAutoString(NS_STATIC_CAST(const char*, str)));

    nsCOMPtr<nsIContent> root = getter_AddRefs(mBindingDocument->GetRootContent());
    if (root)
      nsXBLService::StripWhitespaceNodes(root);
    else {
      NS_ERROR("*** XBL doc with no root element! Something went horribly wrong! ***");
      return NS_ERROR_FAILURE;
    }

    // Put our doc in the doc table.
    nsCOMPtr<nsIXBLDocumentInfo> info;
    NS_NewXBLDocumentInfo(mBindingDocument, getter_AddRefs(info));
 
    // Construct our prototype handlers.
    nsXBLService::ConstructPrototypeHandlers(info);

    // If the doc is a chrome URI, then we put it into the XUL cache.
    PRBool cached = PR_FALSE;
    if (IsChromeURI(uri) && gXULUtils->UseXULCache()) {
      cached = PR_TRUE;
      gXULCache->PutXBLDocumentInfo(info);

      // Cache whether or not this chrome XBL can execute scripts.
      nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID, &rv));
      if (NS_SUCCEEDED(rv) && reg) {
        PRBool allow = PR_TRUE;
        reg->AllowScriptsForSkin(uri, &allow);
        info->SetScriptAccess(allow);
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
    delete req;
  }

  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(mBindingDocument));
  rec->RemoveEventListener(NS_ConvertASCIItoUCS2("load"), (nsIDOMLoadListener*)this, PR_FALSE);

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
      PRUint32 readCount = 0;
      while (mIndex < mSize && aCount > 0) {
          *aBuf = mBuffer[mIndex];
          aBuf++;
          mIndex++;
          readCount++;
          aCount--;
      }
      *aReadCount = readCount;
      return NS_OK;
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMPL_ISUPPORTS(nsProxyStream, NS_GET_IID(nsIInputStream));


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
nsIXULContentUtils* nsXBLService::gXULUtils = nsnull;
nsIXULPrototypeCache* nsXBLService::gXULCache = nsnull;
 
nsINameSpaceManager* nsXBLService::gNameSpaceManager = nsnull;
 
nsHashtable* nsXBLService::gClassTable = nsnull;

JSCList  nsXBLService::gClassLRUList = JS_INIT_STATIC_CLIST(&nsXBLService::gClassLRUList);
PRUint32 nsXBLService::gClassLRUListLength = 0;
PRUint32 nsXBLService::gClassLRUListQuota = 64;

nsIAtom* nsXBLService::kExtendsAtom = nsnull;
nsIAtom* nsXBLService::kHandlersAtom = nsnull;
nsIAtom* nsXBLService::kScrollbarAtom = nsnull;
nsIAtom* nsXBLService::kInputAtom = nsnull;

// Enabled by default. Must be over-ridden to disable
PRBool nsXBLService::gDisableChromeCache = PR_FALSE;
static const char kDisableChromeCachePref[] = "nglayout.debug.disable_xul_cache";

PRInt32 nsXBLService::kNameSpaceID_XBL;

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsXBLService, nsIXBLService, nsIMemoryPressureObserver)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  NS_INIT_REFCNT();
  mPool.Init("XBL Binding Requests", kBucketSizes, kNumBuckets, kInitialSize);

  gRefCnt++;
  if (gRefCnt == 1) {
    
    // Register the XBL namespace.
    nsresult rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                     nsnull,
                                                     NS_GET_IID(nsINameSpaceManager),
                                                     (void**) &gNameSpaceManager);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
    if (NS_FAILED(rv)) return;

    // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
    static const char kXBLNameSpaceURI[]
        = "http://www.mozilla.org/xbl";

    rv = gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXBLNameSpaceURI), kNameSpaceID_XBL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XBL namespace");
    if (NS_FAILED(rv)) return;

    // Create our atoms
    kExtendsAtom = NS_NewAtom("extends");
    kHandlersAtom = NS_NewAtom("handlers");
    kScrollbarAtom = NS_NewAtom("scrollbar");
    kInputAtom = NS_NewAtom("input");

    // Find out if the XUL cache is on or off
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    if (NS_SUCCEEDED(rv))
      prefs->GetBoolPref(kDisableChromeCachePref, &gDisableChromeCache);

    gClassTable = new nsHashtable();

    // Register the first (and only) nsXBLService as a memory pressure observer
    // so it can flush the LRU list in low-memory situations.
    nsMemory::RegisterObserver(this);

    rv = nsServiceManager::GetService("component://netscape/rdf/xul-content-utils",
                                      NS_GET_IID(nsIXULContentUtils),
                                      (nsISupports**) &gXULUtils);
    if (NS_FAILED(rv)) return;

    rv = nsServiceManager::GetService("component://netscape/rdf/xul-prototype-cache",
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
    NS_RELEASE(kExtendsAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kScrollbarAtom);
    NS_RELEASE(kInputAtom);

    // Walk the LRU list removing and deleting the nsXBLJSClasses.
    FlushMemory(REASON_HEAP_MINIMIZE, 0);

    // Any straggling nsXBLJSClass instances held by unfinalized JS objects
    // created for bindings will be deleted when those objects are finalized
    // (and not put on gClassLRUList, because length >= quota).
    gClassLRUListLength = gClassLRUListQuota = 0;

    // At this point, the only hash table entries should be for referenced
    // XBL class structs held by unfinalized JS binding objects.
    delete gClassTable;
    gClassTable = nsnull;

    nsMemory::UnregisterObserver(this);

    if (gXULUtils) {
      nsServiceManager::ReleaseService("component://netscape/rdf/xul-content-utils", gXULUtils);
      gXULUtils = nsnull;
    }

    if (gXULCache) {
      nsServiceManager::ReleaseService("component://netscape/rdf/xul-prototype-cache", gXULCache);
      gXULCache = nsnull;
    }
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, const nsAReadableString& aURL, PRBool aAugmentFlag,
                           nsIXBLBinding** aBinding) 
{ 
  *aBinding = nsnull;

  nsresult rv;

  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
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
        if (uri.EqualsWithConversion((const PRUnichar *) nsPromiseFlatString(aURL)))
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
    NS_ERROR(str);
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
    // InstallProperties that the whole chain should just be whacked and rebuilt.
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
  newBinding->GenerateAnonymousContent(aContent);

  // Tell the binding to install event handlers
  newBinding->InstallEventHandlers(aContent, aBinding);

  // Set up our properties
  newBinding->InstallProperties(aContent);

  return NS_OK; 
}

// For a given element, returns a flat list of all the anonymous children that need
// frames built.
NS_IMETHODIMP
nsXBLService::GetContentList(nsIContent* aContent, nsISupportsArray** aResult, nsIContent** aParent, 
                             PRBool* aMultipleInsertionPoints)
{ 
  // Iterate over all of the bindings one by one and build up an array
  // of anonymous items.
  *aResult = nsnull;
  *aParent = nsnull;
  *aMultipleInsertionPoints = PR_FALSE;

  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
  nsCOMPtr<nsIBindingManager> bindingManager;
  NS_ASSERTION(document, "no document");
  if (!document) return NS_ERROR_FAILURE;
  document->GetBindingManager(getter_AddRefs(bindingManager));
  
  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(aContent, getter_AddRefs(binding));
    
  while (binding) {
    // Get the anonymous content.
    nsCOMPtr<nsIContent> content;
    binding->GetAnonymousContent(getter_AddRefs(content));
    if (content) {
      PRInt32 childCount;
      content->ChildCount(childCount);
      for (PRInt32 i = 0; i < childCount; i++) {
        nsCOMPtr<nsIContent> anonymousChild;
        content->ChildAt(i, *getter_AddRefs(anonymousChild));
        if (!(*aResult)) 
          NS_NewISupportsArray(aResult); // This call addrefs the array.

        (*aResult)->AppendElement(anonymousChild);
      }

      binding->GetSingleInsertionPoint(aParent, aMultipleInsertionPoints);
      return NS_OK;
    }

    nsCOMPtr<nsIXBLBinding> nextBinding;
    binding->GetBaseBinding(getter_AddRefs(nextBinding));
    binding = nextBinding;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::FlushStyleBindings(nsIContent* aContent)
{
  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
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
  if (gXULUtils->UseXULCache()) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(aURLStr, aResult);
  }

  if (!*aResult) {
    // The second line of defense is the binding manager's document table.
    nsCOMPtr<nsIDocument> boundDocument;
    aBoundElement->GetDocument(*getter_AddRefs(boundDocument));
    nsCOMPtr<nsIBindingManager> bindingManager;
    boundDocument->GetBindingManager(getter_AddRefs(bindingManager));
    bindingManager->GetXBLDocumentInfo(aURLStr, aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::FlushMemory(PRUint32 reason, size_t requestedAmount)
{
  while (!JS_CLIST_IS_EMPTY(&gClassLRUList)) {
    JSCList* lru = gClassLRUList.next;
    nsXBLJSClass* c = NS_STATIC_CAST(nsXBLJSClass*, lru);

    JS_REMOVE_AND_INIT_LINK(lru);
    delete c;
    gClassLRUListLength--;

    if (reason == REASON_ALLOC_FAILURE) {
      if (requestedAmount <= sizeof(nsXBLJSClass))
        break;
      requestedAmount -= sizeof(nsXBLJSClass);
    }
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

  // We have a doc. Obtain our specific binding element.
  // Walk the children looking for the binding that matches the ref
  // specified in the URL.
  nsCOMPtr<nsIContent> root = getter_AddRefs(doc->GetRootContent());
  if (!root)
    return NS_ERROR_FAILURE;

  nsAutoString bindingName; bindingName.AssignWithConversion( NS_STATIC_CAST(const char*, ref) );

  PRInt32 count;
  root->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;
    root->ChildAt(i, *getter_AddRefs(child));

    nsAutoString value;
    child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value);
    
    // If no ref is specified just use this.
    if ((bindingName.IsEmpty()) || (bindingName == value)) {
      // Check for the presence of an extends attribute
      nsAutoString extends;
      nsCOMPtr<nsIXBLBinding> baseBinding;
      child->GetAttribute(kNameSpaceID_None, kExtendsAtom, extends);
      value = extends;
      if (!extends.IsEmpty()) {
        nsAutoString prefix;
        PRInt32 offset = extends.FindChar(':');
        if (-1 != offset) {
          extends.Left(prefix, offset);
          extends.Cut(0, offset+1);
        }
        if (prefix.Length() > 0) {
          // Look up the prefix.
          nsCOMPtr<nsIAtom> prefixAtom = getter_AddRefs(NS_NewAtom(prefix));
          nsCOMPtr<nsINameSpace> nameSpace;
          nsCOMPtr<nsIXMLContent> xmlContent(do_QueryInterface(child));
          if (xmlContent) {
            xmlContent->GetContainingNameSpace(*getter_AddRefs(nameSpace));
            if (nameSpace) {
              nsCOMPtr<nsINameSpace> tagSpace;
              nameSpace->FindNameSpace(prefixAtom, *getter_AddRefs(tagSpace));
              if (!tagSpace) {
                // We have a base class binding. Load it right now.
                nsCAutoString urlCString; urlCString.AssignWithConversion(value);
                GetBindingInternal(aBoundElement, urlCString, aPeekOnly, aIsReady, getter_AddRefs(baseBinding));
                if (!*aIsReady)
                  return NS_ERROR_FAILURE; // Binding not yet ready or an error occurred.
              }
            }
          }
        }
      }

      *aIsReady = PR_TRUE;
      if (!aPeekOnly) {
        // Make a new binding
        NS_NewXBLBinding(uri, ref, aResult);

        // Initialize its bound element.
        (*aResult)->SetBindingElement(child);
        (*aResult)->SetAllowScripts(allowScripts);

        if (baseBinding)
          (*aResult)->SetBaseBinding(baseBinding);
      }
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                      const nsCString& aURLStr, const nsCString& aRef,
                                      PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult)
{
  nsresult rv;

  *aResult = nsnull;
  
  // We've got a file.  Check our XBL document cache.
  nsCOMPtr<nsIXBLDocumentInfo> info;
  if (gXULUtils->UseXULCache()) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so that any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(aURLStr, getter_AddRefs(info));
  }

  if (!info) {
    // The second line of defense is the binding manager's document table.
    nsCOMPtr<nsIBindingManager> bindingManager;
    aBoundDocument->GetBindingManager(getter_AddRefs(bindingManager));
    bindingManager->GetXBLDocumentInfo(aURLStr, getter_AddRefs(info));

    nsCOMPtr<nsIAtom> tagName;
    if (aBoundElement)
      aBoundElement->GetTag(*getter_AddRefs(tagName));
    if (!info && (tagName.get() != kScrollbarAtom) && (tagName.get() != kInputAtom) 
        && !aForceSyncLoad) {
      // The third line of defense is to investigate whether or not the
      // document is currently being loaded asynchronously.  If so, there's no
      // document yet, but we need to glom on our request so that it will be
      // processed whenever the doc does finish loading.
      nsCOMPtr<nsIStreamListener> listener;
      bindingManager->GetLoadingDocListener(aURLStr, getter_AddRefs(listener));
      if (listener) {
        nsIStreamListener* ilist = listener.get();
        nsXBLStreamListener* xblListener = NS_STATIC_CAST(nsXBLStreamListener*, ilist);
        // Create a new load observer.
        nsCAutoString bindingURI(aURLStr);
        bindingURI += "#";
        bindingURI += aRef;
        if (!xblListener->HasRequest(bindingURI, aBoundElement)) {
          nsXBLBindingRequest* req = new (mPool) nsXBLBindingRequest(bindingURI, aBoundElement);
          xblListener->AddRequest(req);
        }
        return NS_OK;
      }
    }
     
    if (!info) {
      // Finally, if all lines of defense fail, we go and fetch the binding
      // document.
      nsCOMPtr<nsIURL> uri;
      nsComponentManager::CreateInstance("component://netscape/network/standard-url",
                                         nsnull,
                                         NS_GET_IID(nsIURL),
                                         getter_AddRefs(uri));
      uri->SetSpec(aURLStr);
      nsCOMPtr<nsIDocument> document;
      FetchBindingDocument(aBoundElement, aBoundDocument, uri, aRef, aForceSyncLoad, getter_AddRefs(document));
   
      if (document) {
        NS_NewXBLDocumentInfo(document, getter_AddRefs(info));

        // Construct our prototype handlers.
        ConstructPrototypeHandlers(info);
        
        // If the doc is a chrome URI, then we put it into the XUL cache.
        PRBool cached = PR_FALSE;
        if (IsChromeURI(uri) && gXULUtils->UseXULCache()) {
          cached = PR_TRUE;
          gXULCache->PutXBLDocumentInfo(info);

          // Cache whether or not this chrome XBL can execute scripts.
          nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID, &rv));
          if (NS_SUCCEEDED(rv) && reg) {
            PRBool allow = PR_TRUE;
            reg->AllowScriptsForSkin(uri, &allow);
            info->SetScriptAccess(allow);
          }
        }
        
        if (!cached) {
          // Otherwise we put it in our binding manager's document table.
          nsCOMPtr<nsIBindingManager> bindingManager;
          aBoundDocument->GetBindingManager(getter_AddRefs(bindingManager));
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
  aBoundDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  
  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), aURI, nsnull, loadGroup);
  if (NS_FAILED(rv)) return rv;

  // Call StartDocumentLoad
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = xmlDoc->StartDocumentLoad("loadAsData", channel, 
                                               loadGroup, nsnull, getter_AddRefs(listener)))) {
    NS_ERROR("Failure to init XBL doc prior to load.");
    return rv;
  }

  nsCOMPtr<nsIAtom> tagName;
  if (aBoundElement)
    aBoundElement->GetTag(*getter_AddRefs(tagName)); 
  if ((tagName.get() != kScrollbarAtom) && (tagName.get() != kInputAtom) && !aForceSyncLoad) {
    // We can be asynchronous
    nsXBLStreamListener* xblListener = new nsXBLStreamListener(listener, aBoundDocument, doc);
    
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(doc));
    rec->AddEventListener(NS_ConvertASCIItoUCS2("load"), (nsIDOMLoadListener*)xblListener, PR_FALSE);

    // Add ourselves to the list of loading docs.
    nsCOMPtr<nsIBindingManager> bindingManager;
    aBoundDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsXPIDLCString uri;
    aURI->GetSpec(getter_Copies(uri));
    bindingManager->PutLoadingDocListener(nsCAutoString(NS_STATIC_CAST(const char*, uri)), xblListener);

    // Add our request.
    nsCAutoString bindingURI(uri);
    bindingURI += "#";
    bindingURI += aRef;
    nsXBLBindingRequest* req = new (mPool) nsXBLBindingRequest(bindingURI, aBoundElement);
    xblListener->AddRequest(req);

    // Now kick off the async read.
    channel->AsyncRead(xblListener, nsnull);
    return NS_OK;
  }

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIInputStream> in;
  PRUint32 sourceOffset = 0;
  rv = channel->OpenInputStream(getter_AddRefs(in));

  // If we couldn't open the channel, then just return.
  if (NS_FAILED(rv)) return NS_OK;

  NS_ASSERTION(in != nsnull, "no input stream");
  if (! in) return NS_ERROR_FAILURE;

  rv = NS_ERROR_OUT_OF_MEMORY;
  nsProxyStream* proxy = new nsProxyStream();
  if (! proxy)
    return NS_ERROR_FAILURE;

  listener->OnStartRequest(channel, nsnull);
  while (PR_TRUE) {
    char buf[1024];
    PRUint32 readCount;

    if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
        break; // error

    if (readCount == 0)
        break; // eof

    proxy->SetBuffer(buf, readCount);

    rv = listener->OnDataAvailable(channel, nsnull, proxy, sourceOffset, readCount);
    sourceOffset += readCount;
    if (NS_FAILED(rv))
        break;
  }
  listener->OnStopRequest(channel, nsnull, NS_OK, nsnull);

  // don't leak proxy!
  proxy->Close();
  delete proxy;

  // The document is parsed. We now have a prototype document.
  // Everything worked, so we can just hand this back now.
  *aResult = doc;
  NS_IF_ADDREF(*aResult);

  // The XML content sink produces a ridiculous # of content nodes.
  // It generates text nodes even for whitespace.  The following
  // call walks the generated document tree and trims out these
  // nodes.
  nsCOMPtr<nsIContent> root = getter_AddRefs(doc->GetRootContent());
  if (root)
    StripWhitespaceNodes(root);

  return NS_OK;
}

nsresult 
nsXBLService::StripWhitespaceNodes(nsIContent* aElement)
{
  PRInt32 childCount;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsITextContent> text = do_QueryInterface(child);
    if (text) {
      nsAutoString result;
      text->CopyText(result);
      result.StripWhitespace();
      if (result.IsEmpty()) {
        // This node contained nothing but whitespace.
        // Remove it from the content model.
        aElement->RemoveChildAt(i, PR_TRUE);
        i--; // Decrement our count, since we just removed this child.
        childCount--; // Also decrement our total count.
      }
    }
    else StripWhitespaceNodes(child);
  }

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

nsresult 
nsXBLService::ConstructPrototypeHandlers(nsIXBLDocumentInfo* aInfo)
{
  nsCOMPtr<nsIDocument> doc;
  aInfo->GetDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIContent> bindings = getter_AddRefs(doc->GetRootContent());
  PRInt32 childCount;
  bindings->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> binding;
    bindings->ChildAt(i, *getter_AddRefs(binding));

    // See if this binding has a handler elt.
    nsCOMPtr<nsIContent> handlers;
    GetImmediateChild(kHandlersAtom, binding, getter_AddRefs(handlers));
    if (handlers) {
      nsCOMPtr<nsIXBLPrototypeHandler> firstHandler;
      nsCOMPtr<nsIXBLPrototypeHandler> currHandler;

      PRInt32 handlerCount;
      handlers->ChildCount(handlerCount);
      for (PRInt32 j = 0; j < handlerCount; j++) {
        nsCOMPtr<nsIContent> handler;
        handlers->ChildAt(j, *getter_AddRefs(handler));
        
        nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
        NS_NewXBLPrototypeHandler(handler, getter_AddRefs(newHandler));
        if (newHandler) {
          if (currHandler)
            currHandler->SetNextHandler(newHandler);
          else firstHandler = newHandler;
          currHandler = newHandler;
        }
      }

      if (firstHandler) {
        nsAutoString ref;
        binding->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, ref);
        
        nsCAutoString cref;
        cref.AssignWithConversion(ref);

        aInfo->SetPrototypeHandler(cref, firstHandler);
      }
    }
  }

  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLService(nsIXBLService** aResult)
{
  *aResult = new nsXBLService;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

