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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   - Brendan Eich (brendan@mozilla.org)
 *   - Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsXBLService.h"
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
#include "nsXBLPrototypeBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsXBLAtoms.h"
#include "nsXULAtoms.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsISyncLoadDOMService.h"
#include "nsIDOM3Node.h"

#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsIScriptSecurityManager.h"

#ifdef MOZ_XUL
#include "nsIXULPrototypeCache.h"
#endif
#include "nsIDOMLoadListener.h"
#include "nsIDOMEventGroup.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);

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
  nsCOMPtr<nsIURL> mBindingURL;
  nsCOMPtr<nsIContent> mBoundElement;

  static nsXBLBindingRequest*
  Create(nsFixedSizeAllocator& aPool, nsIURL* aURL, nsIContent* aBoundElement) {
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
    nsCOMPtr<nsIDocument> doc = mBoundElement->GetDocument();
    if (!doc)
      return;

    // Get the binding.
    PRBool ready = PR_FALSE;
    gXBLService->BindingReady(mBoundElement, mBindingURL, &ready);

    if (!ready)
      return;

    // XXX Deal with layered bindings.
    // Now do a ContentInserted notification to cause the frames to get installed finally,
    nsCOMPtr<nsIContent> parent = mBoundElement->GetParent();
    PRInt32 index = 0;
    if (parent)
      index = parent->IndexOf(mBoundElement);
        
    // If |mBoundElement| is (in addition to having binding |mBinding|)
    // also a descendant of another element with binding |mBinding|,
    // then we might have just constructed it due to the
    // notification of its parent.  (We can know about both if the
    // binding loads were triggered from the DOM rather than frame
    // construction.)  So we have to check both whether the element
    // has a primary frame and whether it's in the undisplayed map
    // before sending a ContentInserted notification, or bad things
    // will happen.
    nsIPresShell *shell = doc->GetShellAt(0);
    if (shell) {
      nsIFrame* childFrame;
      shell->GetPrimaryFrameFor(mBoundElement, &childFrame);
      if (!childFrame) {
        // Check to see if it's in the undisplayed content map.
        nsStyleContext* sc =
          shell->FrameManager()->GetUndisplayedContent(mBoundElement);

        if (!sc) {
          nsCOMPtr<nsIDocumentObserver> obs(do_QueryInterface(shell));
          obs->ContentInserted(doc, parent, mBoundElement, index);
        }
      }
    }
  }

  static nsIXBLService* gXBLService;
  static int gRefCnt;

protected:
  nsXBLBindingRequest(nsIURL* aURL, nsIContent* aBoundElement)
    : mBindingURL(aURL),
      mBoundElement(aBoundElement)
  {
    gRefCnt++;
    if (gRefCnt == 1) {
      CallGetService("@mozilla.org/xbl;1", &gXBLService);
    }
  }

  ~nsXBLBindingRequest()
  {
    gRefCnt--;
    if (gRefCnt == 0) {
      NS_IF_RELEASE(gXBLService);
    }
  }

private:
  // Hide so that only Create() and Destroy() can be used to
  // allocate and deallocate from the heap
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
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
  NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD Unload(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD Abort(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD Error(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };

#ifdef MOZ_XUL
  static nsIXULPrototypeCache* gXULCache;
  static PRInt32 gRefCnt;
#endif

  nsXBLStreamListener(nsXBLService* aXBLService, nsIStreamListener* aInner, nsIDocument* aDocument, nsIDocument* aBindingDocument);
  virtual ~nsXBLStreamListener();
  
  void AddRequest(nsXBLBindingRequest* aRequest) { mBindingRequests.AppendElement(aRequest); };
  PRBool HasRequest(nsIURI* aURI, nsIContent* aBoundElement);

private:
  nsXBLService* mXBLService; // [WEAK]

  nsCOMPtr<nsIStreamListener> mInner;
  nsAutoVoidArray mBindingRequests;
  
  nsCOMPtr<nsIWeakReference> mDocument;
  nsCOMPtr<nsIDocument> mBindingDocument;
};

#ifdef MOZ_XUL
nsIXULPrototypeCache* nsXBLStreamListener::gXULCache = nsnull;
PRInt32 nsXBLStreamListener::gRefCnt = 0;
#endif

/* Implementation file */
NS_IMPL_ISUPPORTS4(nsXBLStreamListener, nsIStreamListener, nsIRequestObserver, nsIDOMLoadListener, nsIDOMEventListener)

nsXBLStreamListener::nsXBLStreamListener(nsXBLService* aXBLService,
                                         nsIStreamListener* aInner, nsIDocument* aDocument,
                                         nsIDocument* aBindingDocument)
{
  /* member initializers and constructor code */
  mXBLService = aXBLService;
  mInner = aInner;
  mDocument = do_GetWeakReference(aDocument);
  mBindingDocument = aBindingDocument;
#ifdef MOZ_XUL
  gRefCnt++;
  if (gRefCnt == 1) {
    nsresult rv = CallGetService("@mozilla.org/xul/xul-prototype-cache;1",
                                 &gXULCache);
    if (NS_FAILED(rv)) return;
  }
#endif
}

nsXBLStreamListener::~nsXBLStreamListener()
{
#ifdef MOZ_XUL
  /* destructor code */
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_IF_RELEASE(gXULCache);
  }
#endif
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
      nsCAutoString str;
      channelURI->GetAsciiSpec(str);
      printf("Failed to load XBL document %s\n", str.get());
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
nsXBLStreamListener::HasRequest(nsIURI* aURI, nsIContent* aElt)
{
  // XXX Could be more efficient.
  PRUint32 count = mBindingRequests.Count();
  for (PRUint32 i = 0; i < count; i++) {
    nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
    PRBool eq;
    if (req->mBoundElement == aElt &&
        NS_SUCCEEDED(req->mBindingURL->Equals(aURI, &eq)) && eq)
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
      nsIDocument* document = req->mBoundElement->GetDocument();
      if (document)
        document->FlushPendingNotifications(Flush_ContentAndNotify);
    }

    // Remove ourselves from the set of pending docs.
    nsIBindingManager *bindingManager = doc->GetBindingManager();
    nsIURI* documentURI = mBindingDocument->GetDocumentURI();
    bindingManager->RemoveLoadingDocListener(documentURI);

    if (!mBindingDocument->GetRootContent()) {
      NS_ERROR("*** XBL doc with no root element! Something went horribly wrong! ***");
      return NS_ERROR_FAILURE;
    }

    // Put our doc info in the doc table.
    nsCOMPtr<nsIXBLDocumentInfo> info;
    nsIBindingManager *xblDocBindingManager = mBindingDocument->GetBindingManager();
    xblDocBindingManager->GetXBLDocumentInfo(documentURI, getter_AddRefs(info));
    xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.
    if (!info) {
      NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
      return NS_ERROR_FAILURE;
    }

    // If the doc is a chrome URI, then we put it into the XUL cache.
#ifdef MOZ_XUL
    if (IsChromeOrResourceURI(documentURI)) {
      PRBool useXULCache;
      gXULCache->GetEnabled(&useXULCache);
      if (useXULCache)
        gXULCache->PutXBLDocumentInfo(info);
    }
#endif
  
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
    // XXXbz this is still needed for <xul:iframe>.  We need to fix that.
    if (count > 0) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(0);
      nsIDocument* document = req->mBoundElement->GetDocument();
      if (document)
        document->FlushPendingNotifications(Flush_Layout);
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

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
#ifdef MOZ_XUL
nsIXULPrototypeCache* nsXBLService::gXULCache = nsnull;
#endif
 
nsHashtable* nsXBLService::gClassTable = nsnull;

JSCList  nsXBLService::gClassLRUList = JS_INIT_STATIC_CLIST(&nsXBLService::gClassLRUList);
PRUint32 nsXBLService::gClassLRUListLength = 0;
PRUint32 nsXBLService::gClassLRUListQuota = 64;

// Enabled by default. Must be over-ridden to disable
PRBool nsXBLService::gDisableChromeCache = PR_FALSE;
static const char kDisableChromeCachePref[] = "nglayout.debug.disable_xul_cache";

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsXBLService, nsIXBLService, nsIObserver, nsISupportsWeakReference)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  mPool.Init("XBL Binding Requests", kBucketSizes, kNumBuckets, kInitialSize);

  gRefCnt++;
  if (gRefCnt == 1) {
    gClassTable = new nsHashtable();

#ifdef MOZ_XUL
    // Find out if the XUL cache is on or off
    gDisableChromeCache = nsContentUtils::GetBoolPref(kDisableChromeCachePref,
                                                      gDisableChromeCache);

    CallGetService("@mozilla.org/xul/xul-prototype-cache;1", &gXULCache);
#endif
  }
}

nsXBLService::~nsXBLService(void)
{
  gRefCnt--;
  if (gRefCnt == 0) {
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

#ifdef MOZ_XUL
    NS_IF_RELEASE(gXULCache);
#endif
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, nsIURI* aURL, PRBool aAugmentFlag,
                           nsIXBLBinding** aBinding, PRBool* aResolveStyle) 
{ 
  *aBinding = nsnull;
  *aResolveStyle = PR_FALSE;

  nsresult rv;

  nsCOMPtr<nsIDocument> document = aContent->GetDocument();

  // XXX document may be null if we're in the midst of paint suppression
  if (!document)
    return NS_OK;

  nsIBindingManager *bindingManager = document->GetBindingManager();
  
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
        nsIURI* uri = styleBinding->BindingURI();
        PRBool equal;
        if (NS_SUCCEEDED(uri->Equals(aURL, &equal)) && equal)
          return NS_OK;
        FlushStyleBindings(aContent);
        binding = nsnull;
      }
    }
  }

  // Security check - remote pages can't load local bindings, except from chrome
  nsIURI *docURI = document->GetDocumentURI();
  PRBool isChrome = PR_FALSE;
  rv = docURI->SchemeIs("chrome", &isChrome);

  // Not everything with a chrome URI has a system principal.  See bug 160042.
  if (NS_FAILED(rv) || !isChrome) {
    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

    rv = secMan->
      CheckLoadURIWithPrincipal(document->GetPrincipal(), aURL,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv))
      return rv;
  }
  nsCOMPtr<nsIXBLBinding> newBinding;
  if (NS_FAILED(rv = GetBinding(aContent, aURL, getter_AddRefs(newBinding)))) {
    return rv;
  }

  if (!newBinding) {
#ifdef DEBUG
    nsCAutoString spec;
    aURL->GetSpec(spec);
    nsCAutoString str(NS_LITERAL_CSTRING("Failed to locate XBL binding. XBL is now using id instead of name to reference bindings. Make sure you have switched over.  The invalid binding name is: ") + spec);
    NS_ERROR(str.get());
#endif
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

nsresult
nsXBLService::FlushStyleBindings(nsIContent* aContent)
{
  nsCOMPtr<nsIDocument> document = aContent->GetDocument();

  // XXX doc will be null if we're in the midst of paint suppression.
  if (! document)
    return NS_OK;

  nsIBindingManager *bindingManager = document->GetBindingManager();
  
  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(aContent, getter_AddRefs(binding));
  
  if (binding) {
    nsCOMPtr<nsIXBLBinding> styleBinding;
    binding->GetFirstStyleBinding(getter_AddRefs(styleBinding));

    if (styleBinding) {
      // Clear out the script references.
      styleBinding->UnhookEventHandlers();
      styleBinding->ChangeDocument(document, nsnull);
    }

    if (styleBinding == binding) 
      bindingManager->SetBinding(aContent, nsnull); // Flush old style bindings
  }
   
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID,
                         nsIAtom** aResult)
{
  nsIDocument* document = aContent->GetDocument();
  if (document) {
    nsIBindingManager *bindingManager = document->GetBindingManager();
  
    if (bindingManager)
      return bindingManager->ResolveTag(aContent, aNameSpaceID, aResult);
  }

  aContent->GetNameSpaceID(aNameSpaceID);
  NS_ADDREF(*aResult = aContent->Tag());

  return NS_OK;
}

nsresult
nsXBLService::GetXBLDocumentInfo(nsIURI* aURI, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)
{
  *aResult = nsnull;

#ifdef MOZ_XUL
  PRBool useXULCache;
  gXULCache->GetEnabled(&useXULCache);
  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(aURI, aResult);
  }
#endif

  if (!*aResult) {
    // The second line of defense is the binding manager's document table.
    nsIDocument* boundDocument = aBoundElement->GetDocument();
    if (boundDocument)
      boundDocument->GetBindingManager()->GetXBLDocumentInfo(aURI, aResult);
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
    nsCOMPtr<nsIDocument> doc = contentNode->GetDocument();
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
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  rec->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(rec);

  target->AddGroupedEventListener(NS_LITERAL_STRING("keydown"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("keyup"), handler, 
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("keypress"), handler, 
                                  PR_FALSE, systemGroup);

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

  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  aReceiver->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(aReceiver);

  // listen to these events
  target->AddGroupedEventListener(NS_LITERAL_STRING("draggesture"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("dragenter"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("dragexit"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("dragover"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("dragdrop"), handler,
                                  PR_FALSE, systemGroup);

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

nsresult nsXBLService::GetBinding(nsIContent* aBoundElement, 
                                  nsIURI* aURI, 
                                  nsIXBLBinding** aResult)
{
  PRBool dummy;
  return GetBindingInternal(aBoundElement, aURI, PR_FALSE, &dummy, aResult);
}

NS_IMETHODIMP nsXBLService::BindingReady(nsIContent* aBoundElement, 
                                         nsIURI* aURI, 
                                         PRBool* aIsReady)
{
  return GetBindingInternal(aBoundElement, aURI, PR_TRUE, aIsReady, nsnull);
}

NS_IMETHODIMP nsXBLService::GetBindingInternal(nsIContent* aBoundElement, 
                                               nsIURI* aURI, 
                                               PRBool aPeekOnly,
                                               PRBool* aIsReady, 
                                               nsIXBLBinding** aResult)
{
  if (aResult)
    *aResult = nsnull;

  if (!aURI)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (!url) {
#ifdef DEBUG
    NS_ERROR("Binding load from a non-URL URI not allowed.");
    nsCAutoString spec;
    aURI->GetSpec(spec);
    fprintf(stderr, "Spec of non-URL URI is: '%s'\n", spec.get());
#endif
    return NS_ERROR_FAILURE;
  }

  nsCAutoString ref;
  url->GetRef(ref);
  NS_ASSERTION(!ref.IsEmpty(), "Incorrect syntax for an XBL binding");
  if (ref.IsEmpty())
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDocument> boundDocument = aBoundElement->GetDocument();

  nsCOMPtr<nsIXBLDocumentInfo> docInfo;
  LoadBindingDocumentInfo(aBoundElement, boundDocument, aURI, PR_FALSE,
                          getter_AddRefs(docInfo));
  if (!docInfo)
    return NS_ERROR_FAILURE;

  // Get our doc info and determine our script access.
  nsCOMPtr<nsIDocument> doc;
  docInfo->GetDocument(getter_AddRefs(doc));
  PRBool allowScripts;
  docInfo->GetScriptAccess(&allowScripts);

  nsXBLPrototypeBinding* protoBinding;
  docInfo->GetPrototypeBinding(ref, &protoBinding);

  NS_ASSERTION(protoBinding, "Unable to locate an XBL binding.");
  if (!protoBinding)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> child = protoBinding->GetBindingElement();

  // Our prototype binding must have all its resources loaded.
  PRBool ready = protoBinding->LoadResources();
  if (!ready) {
    // Add our bound element to the protos list of elts that should
    // be notified when the stylesheets and scripts finish loading.
    protoBinding->AddResourceListener(aBoundElement);
    return NS_ERROR_FAILURE; // The binding isn't ready yet.
  }

  // If our prototype already has a base, then don't check for an "extends" attribute.
  nsCOMPtr<nsIXBLBinding> baseBinding;
  PRBool hasBase = protoBinding->HasBasePrototype();
  nsXBLPrototypeBinding* baseProto = protoBinding->GetBasePrototype();
  if (baseProto) {
    if (NS_FAILED(GetBindingInternal(aBoundElement, baseProto->BindingURI(),
                                     aPeekOnly, aIsReady, getter_AddRefs(baseBinding))))
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

      if (!prefix.IsEmpty()) {
        nsCOMPtr<nsIAtom> prefixAtom = do_GetAtom(prefix);

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

            nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(nameSpace,
                                                                  &nameSpaceID);

            nsCOMPtr<nsIAtom> tagName = do_GetAtom(display);
            protoBinding->SetBaseTag(nameSpaceID, tagName);
          }
        }
      }

      if (hasExtends && (hasDisplay || nameSpace.IsEmpty())) {
        // Look up the prefix.
        // We have a base class binding. Load it right now.
        nsCOMPtr<nsIURI> bindingURI;
        nsresult rv =
          NS_NewURI(getter_AddRefs(bindingURI), value,
                    doc->GetDocumentCharacterSet().get(),
                    doc->GetBaseURI());
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (NS_FAILED(GetBindingInternal(aBoundElement, bindingURI, aPeekOnly,
                                         aIsReady, getter_AddRefs(baseBinding))))
          return NS_ERROR_FAILURE; // Binding not yet ready or an error occurred.
        if (!aPeekOnly) {
          // Make sure to set the base prototype.
          baseBinding->GetPrototypeBinding(&baseProto);
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
nsXBLService::LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                      nsIDocument* aBoundDocument,
                                      nsIURI* aBindingURI,
                                      PRBool aForceSyncLoad,
                                      nsIXBLDocumentInfo** aResult)
{
  NS_PRECONDITION(aBindingURI, "Must have a binding URI");
  
  nsresult rv = NS_OK;

  *aResult = nsnull;
  nsCOMPtr<nsIXBLDocumentInfo> info;

  nsCOMPtr<nsIURI> uriClone;
  rv = aBindingURI->Clone(getter_AddRefs(uriClone));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIURL> documentURI(do_QueryInterface(uriClone, &rv));
  NS_ENSURE_TRUE(documentURI, rv);

  documentURI->SetRef(EmptyCString());

#ifdef MOZ_XUL
  // We've got a file.  Check our XBL document cache.
  PRBool useXULCache;
  gXULCache->GetEnabled(&useXULCache);

  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so that any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    gXULCache->GetXBLDocumentInfo(documentURI, getter_AddRefs(info));
  }
#endif

  if (!info) {
    // The second line of defense is the binding manager's document table.
    nsIBindingManager *bindingManager = nsnull;

    nsCOMPtr<nsIURL> bindingURL(do_QueryInterface(aBindingURI, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aBoundDocument) {
      bindingManager = aBoundDocument->GetBindingManager();
      bindingManager->GetXBLDocumentInfo(documentURI, getter_AddRefs(info));
    }

    nsINodeInfo *ni = nsnull;
    if (aBoundElement)
      ni = aBoundElement->GetNodeInfo();

    if (!info && bindingManager &&
        (!ni || !(ni->Equals(nsXULAtoms::scrollbar, kNameSpaceID_XUL) ||
                  ni->Equals(nsXULAtoms::thumb, kNameSpaceID_XUL) ||
                  ((ni->Equals(nsHTMLAtoms::input) ||
                    ni->Equals(nsHTMLAtoms::select)) &&
                   aBoundElement->IsContentOfType(nsIContent::eHTML)))) &&
        !aForceSyncLoad) {
      // The third line of defense is to investigate whether or not the
      // document is currently being loaded asynchronously.  If so, there's no
      // document yet, but we need to glom on our request so that it will be
      // processed whenever the doc does finish loading.
      nsCOMPtr<nsIStreamListener> listener;
      if (bindingManager)
        bindingManager->GetLoadingDocListener(documentURI, getter_AddRefs(listener));
      if (listener) {
        nsIStreamListener* ilist = listener.get();
        nsXBLStreamListener* xblListener = NS_STATIC_CAST(nsXBLStreamListener*, ilist);
        // Create a new load observer.
        if (!xblListener->HasRequest(aBindingURI, aBoundElement)) {
          nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool, bindingURL, aBoundElement);
          xblListener->AddRequest(req);
        }
        return NS_OK;
      }
    }
     
    if (!info) {
      // Finally, if all lines of defense fail, we go and fetch the binding
      // document.
      
      // Always load chrome synchronously
      PRBool chrome;
      if (NS_SUCCEEDED(documentURI->SchemeIs("chrome", &chrome)) && chrome)
        aForceSyncLoad = PR_TRUE;

      nsCOMPtr<nsIDocument> document;
      FetchBindingDocument(aBoundElement, aBoundDocument, documentURI,
                           bindingURL, aForceSyncLoad, getter_AddRefs(document));
   
      if (document) {
        nsIBindingManager *xblDocBindingManager = document->GetBindingManager();
        xblDocBindingManager->GetXBLDocumentInfo(documentURI, getter_AddRefs(info));
        if (!info) {
          NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
          return NS_ERROR_FAILURE;
        }
        xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.

        // If the doc is a chrome URI, then we put it into the XUL cache.
#ifdef MOZ_XUL
        if (IsChromeOrResourceURI(documentURI)) {
          if (useXULCache)
            gXULCache->PutXBLDocumentInfo(info);
        }
#endif
        
        if (bindingManager) {
          // Also put it in our binding manager's document table.
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

nsresult
nsXBLService::FetchBindingDocument(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                   nsIURI* aDocumentURI, nsIURL* aBindingURL, 
                                   PRBool aForceSyncLoad, nsIDocument** aResult)
{
  nsresult rv = NS_OK;
  // Initialize our out pointer to nsnull
  *aResult = nsnull;

  // Now we have to synchronously load the binding file.
  // Create an XML content sink and a parser. 
  nsCOMPtr<nsILoadGroup> loadGroup;
  if (aBoundDocument)
    loadGroup = aBoundDocument->GetDocumentLoadGroup();

  nsINodeInfo *ni = nsnull;
  if (aBoundElement)
    ni = aBoundElement->GetNodeInfo();

  if (ni && (ni->Equals(nsXULAtoms::scrollbar, kNameSpaceID_XUL) ||
             ni->Equals(nsXULAtoms::thumb, kNameSpaceID_XUL) || 
             (ni->Equals(nsHTMLAtoms::select) &&
              aBoundElement->IsContentOfType(nsIContent::eHTML))) ||
      IsResourceURI(aDocumentURI))
    aForceSyncLoad = PR_TRUE;

  if(!aForceSyncLoad) {
    // Create the XML document
    nsCOMPtr<nsIDocument> doc = do_CreateInstance(kXMLDocumentCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), aDocumentURI, nsnull, loadGroup);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIXMLContentSink> xblSink;
    NS_NewXBLContentSink(getter_AddRefs(xblSink), doc, aDocumentURI, nsnull);
    if (!xblSink)
      return NS_ERROR_FAILURE;

    if (NS_FAILED(rv = doc->StartDocumentLoad("loadAsInteractiveData", 
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
    nsIBindingManager *bindingManager;
    if (aBoundDocument)
      bindingManager = aBoundDocument->GetBindingManager();
    else
      bindingManager = nsnull;

    if (bindingManager)
      bindingManager->PutLoadingDocListener(aDocumentURI, xblListener);

    // Add our request.
    nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool,
                                                           aBindingURL,
                                                           aBoundElement);
    xblListener->AddRequest(req);

    // Now kick off the async read.
    channel->AsyncOpen(xblListener, nsnull);
    return NS_OK;
  }

  // Now do a blocking synchronous parse of the file.

  nsCOMPtr<nsIDOMDocument> domDoc;
  nsCOMPtr<nsISyncLoadDOMService> loader =
    do_GetService("@mozilla.org/content/syncload-dom-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Open channel
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aDocumentURI, nsnull, loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = loader->LoadLocalXBLDocument(channel, getter_AddRefs(domDoc));
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
      return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(domDoc, aResult);
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

