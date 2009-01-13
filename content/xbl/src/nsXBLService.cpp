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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nsIInputStream.h"
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
#include "nsGkAtoms.h"
#include "nsIMemory.h"
#include "nsIObserverService.h"
#include "nsIDOMNodeList.h"
#include "nsXBLContentSink.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsSyncLoadService.h"
#include "nsIDOM3Node.h"
#include "nsContentPolicyUtils.h"

#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptError.h"

#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif
#include "nsIDOMLoadListener.h"
#include "nsIDOMEventGroup.h"

#define NS_MAX_XBL_BINDING_RECURSION 20

static PRBool IsChromeOrResourceURI(nsIURI* aURI)
{
  PRBool isChrome = PR_FALSE;
  PRBool isResource = PR_FALSE;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && 
      NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)))
      return (isChrome || isResource);
  return PR_FALSE;
}

static PRBool
IsAncestorBinding(nsIDocument* aDocument,
                  nsIURI* aChildBindingURI,
                  nsIContent* aChild)
{
  NS_ASSERTION(aDocument, "expected a document");
  NS_ASSERTION(aChildBindingURI, "expected a binding URI");
  NS_ASSERTION(aChild, "expected a child content");

  PRUint32 bindingRecursion = 0;
  nsBindingManager* bindingManager = aDocument->BindingManager();
  for (nsIContent *bindingParent = aChild->GetBindingParent();
       bindingParent;
       bindingParent = bindingParent->GetBindingParent()) {
    nsXBLBinding* binding = bindingManager->GetBinding(bindingParent);
    if (!binding) {
      continue;
    }
    PRBool equal;
    nsresult rv;
    nsCOMPtr<nsIURL> childBindingURL = do_QueryInterface(aChildBindingURI);
    nsCAutoString childRef;
    if (childBindingURL &&
        NS_SUCCEEDED(childBindingURL->GetRef(childRef)) &&
        childRef.IsEmpty()) {
      // If the child URL has no ref, we need to strip away the ref from the
      // URI we're comparing it to, since the child URL will end up pointing
      // to the first binding defined at its URI, and that could be the same
      // binding that's referred to more specifically by the already attached
      // binding's URI via its ref.

      // This means we'll get false positives if someone refers to the first
      // binding at a given URI without a ref and also binds a parent or child
      // to a different binding at that URI *with* a ref, but that shouldn't
      // ever be necessary so we don't need to support it.
      nsCOMPtr<nsIURI> compareURI;
      rv = binding->PrototypeBinding()->BindingURI()->Clone(getter_AddRefs(compareURI));
      NS_ENSURE_SUCCESS(rv, PR_TRUE); // assume the worst

      nsCOMPtr<nsIURL> compareURL = do_QueryInterface(compareURI, &rv);
      NS_ENSURE_SUCCESS(rv, PR_TRUE); // assume the worst
      
      rv = compareURL->SetRef(EmptyCString());
      NS_ENSURE_SUCCESS(rv, PR_TRUE); // assume the worst

      rv = compareURL->Equals(aChildBindingURI, &equal);
    } else {
      // Just compare the URIs
      rv = binding->PrototypeBinding()->BindingURI()->Equals(aChildBindingURI,
                                                             &equal);
    }

    NS_ENSURE_SUCCESS(rv, PR_TRUE); // assume the worst

    if (equal) {
      ++bindingRecursion;
      if (bindingRecursion < NS_MAX_XBL_BINDING_RECURSION) {
        continue;
      }
      nsCAutoString spec;
      aChildBindingURI->GetSpec(spec);
      NS_ConvertUTF8toUTF16 bindingURI(spec);
      const PRUnichar* params[] = { bindingURI.get() };
      nsContentUtils::ReportToConsole(nsContentUtils::eXBL_PROPERTIES,
                                      "TooDeepBindingRecursion",
                                      params, NS_ARRAY_LENGTH(params),
                                      aDocument->GetDocumentURI(),
                                      EmptyString(), 0, 0,
                                      nsIScriptError::warningFlag,
                                      "XBL");
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool CheckTagNameWhiteList(PRInt32 aNameSpaceID, nsIAtom *aTagName)
{
  static nsIContent::AttrValuesArray kValidXULTagNames[] =  {
    &nsGkAtoms::autorepeatbutton, &nsGkAtoms::box, &nsGkAtoms::browser,
    &nsGkAtoms::button, &nsGkAtoms::hbox, &nsGkAtoms::image, &nsGkAtoms::menu,
    &nsGkAtoms::menubar, &nsGkAtoms::menuitem, &nsGkAtoms::menupopup,
    &nsGkAtoms::row, &nsGkAtoms::slider, &nsGkAtoms::spacer,
    &nsGkAtoms::splitter, &nsGkAtoms::text, &nsGkAtoms::tree, nsnull};

  PRUint32 i;
  if (aNameSpaceID == kNameSpaceID_XUL) {
    for (i = 0; kValidXULTagNames[i]; ++i) {
      if (aTagName == *(kValidXULTagNames[i])) {
        return PR_TRUE;
      }
    }
  }
#ifdef MOZ_SVG
  else if (aNameSpaceID == kNameSpaceID_SVG &&
           aTagName == nsGkAtoms::generic) {
    return PR_TRUE;
  }
#endif

  return PR_FALSE;
}

// Individual binding requests.
class nsXBLBindingRequest
{
public:
  nsCOMPtr<nsIURI> mBindingURI;
  nsCOMPtr<nsIContent> mBoundElement;

  static nsXBLBindingRequest*
  Create(nsFixedSizeAllocator& aPool, nsIURI* aURI, nsIContent* aBoundElement) {
    void* place = aPool.Alloc(sizeof(nsXBLBindingRequest));
    return place ? ::new (place) nsXBLBindingRequest(aURI, aBoundElement) : nsnull;
  }

  static void
  Destroy(nsFixedSizeAllocator& aPool, nsXBLBindingRequest* aRequest) {
    aRequest->~nsXBLBindingRequest();
    aPool.Free(aRequest, sizeof(*aRequest));
  }

  void DocumentLoaded(nsIDocument* aBindingDoc)
  {
    // We only need the document here to cause frame construction, so
    // we need the current doc, not the owner doc.
    nsIDocument* doc = mBoundElement->GetCurrentDoc();
    if (!doc)
      return;

    // Get the binding.
    PRBool ready = PR_FALSE;
    gXBLService->BindingReady(mBoundElement, mBindingURI, &ready);

    if (!ready)
      return;

    // If |mBoundElement| is (in addition to having binding |mBinding|)
    // also a descendant of another element with binding |mBinding|,
    // then we might have just constructed it due to the
    // notification of its parent.  (We can know about both if the
    // binding loads were triggered from the DOM rather than frame
    // construction.)  So we have to check both whether the element
    // has a primary frame and whether it's in the undisplayed map
    // before sending a ContentInserted notification, or bad things
    // will happen.
    nsIPresShell *shell = doc->GetPrimaryShell();
    if (shell) {
      nsIFrame* childFrame = shell->GetPrimaryFrameFor(mBoundElement);
      if (!childFrame) {
        // Check to see if it's in the undisplayed content map.
        nsStyleContext* sc =
          shell->FrameManager()->GetUndisplayedContent(mBoundElement);

        if (!sc) {
          shell->RecreateFramesFor(mBoundElement);
        }
      }
    }
  }

  static nsIXBLService* gXBLService;
  static int gRefCnt;

protected:
  nsXBLBindingRequest(nsIURI* aURI, nsIContent* aBoundElement)
    : mBindingURI(aURI),
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
  NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Unload(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Abort(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Error(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  nsXBLStreamListener(nsXBLService* aXBLService, nsIStreamListener* aInner, nsIDocument* aDocument);
  ~nsXBLStreamListener();

  void AddRequest(nsXBLBindingRequest* aRequest) { mBindingRequests.AppendElement(aRequest); }
  PRBool HasRequest(nsIURI* aURI, nsIContent* aBoundElement);

private:
  nsXBLService* mXBLService; // [WEAK]

  nsCOMPtr<nsIStreamListener> mInner;
  nsAutoVoidArray mBindingRequests;
  
  nsCOMPtr<nsIWeakReference> mDocument;
};

/* Implementation file */
NS_IMPL_ISUPPORTS4(nsXBLStreamListener, nsIStreamListener, nsIRequestObserver, nsIDOMLoadListener, nsIDOMEventListener)

nsXBLStreamListener::nsXBLStreamListener(nsXBLService* aXBLService,
                                         nsIStreamListener* aInner, nsIDocument* aDocument)
{
  /* member initializers and constructor code */
  mXBLService = aXBLService;
  mInner = aInner;
  mDocument = do_GetWeakReference(aDocument);
}

nsXBLStreamListener::~nsXBLStreamListener()
{
  for (PRInt32 i = 0; i < mBindingRequests.Count(); i++) {
    nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
    nsXBLBindingRequest::Destroy(mXBLService->mPool, req);
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

  // Don't hold onto the inner listener; holding onto it can create a cycle
  // with the document
  mInner = nsnull;

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
        NS_SUCCEEDED(req->mBindingURI->Equals(aURI, &eq)) && eq)
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

  // Get the binding document; note that we don't hold onto it in this object
  // to avoid creating a cycle
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetCurrentTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDocument> bindingDocument = do_QueryInterface(target);
  NS_ASSERTION(bindingDocument, "Event not targeted at document?!");

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
      nsIDocument* document = req->mBoundElement->GetCurrentDoc();
      if (document)
        document->FlushPendingNotifications(Flush_ContentAndNotify);
    }

    // Remove ourselves from the set of pending docs.
    nsBindingManager *bindingManager = doc->BindingManager();
    nsIURI* documentURI = bindingDocument->GetDocumentURI();
    bindingManager->RemoveLoadingDocListener(documentURI);

    if (!bindingDocument->GetRootContent()) {
      NS_WARNING("*** XBL doc with no root element! Something went horribly wrong! ***");
      return NS_ERROR_FAILURE;
    }

    // Put our doc info in the doc table.
    nsBindingManager *xblDocBindingManager = bindingDocument->BindingManager();
    nsCOMPtr<nsIXBLDocumentInfo> info =
      xblDocBindingManager->GetXBLDocumentInfo(documentURI);
    xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.
    if (!info) {
      NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
      return NS_ERROR_FAILURE;
    }

    // If the doc is a chrome URI, then we put it into the XUL cache.
#ifdef MOZ_XUL
    if (IsChromeOrResourceURI(documentURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache && cache->IsEnabled())
        cache->PutXBLDocumentInfo(info);
    }
#endif
  
    bindingManager->PutXBLDocumentInfo(info);

    // Notify all pending requests that their bindings are
    // ready and can be installed.
    for (i = 0; i < count; i++) {
      nsXBLBindingRequest* req = (nsXBLBindingRequest*)mBindingRequests.ElementAt(i);
      req->DocumentLoaded(bindingDocument);
    }
  }

  target->RemoveEventListener(NS_LITERAL_STRING("load"), (nsIDOMLoadListener*)this, PR_FALSE);

  return rv;
}

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
PRBool nsXBLService::gAllowDataURIs = PR_FALSE;

nsHashtable* nsXBLService::gClassTable = nsnull;

JSCList  nsXBLService::gClassLRUList = JS_INIT_STATIC_CLIST(&nsXBLService::gClassLRUList);
PRUint32 nsXBLService::gClassLRUListLength = 0;
PRUint32 nsXBLService::gClassLRUListQuota = 64;

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsXBLService, nsIXBLService, nsIObserver, nsISupportsWeakReference)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  mPool.Init("XBL Binding Requests", kBucketSizes, kNumBuckets, kInitialSize);

  gRefCnt++;
  if (gRefCnt == 1) {
    gClassTable = new nsHashtable();
  }
  
  nsContentUtils::AddBoolPrefVarCache("layout.debug.enable_data_xbl",
                                      &gAllowDataURIs);
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
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, nsIURI* aURL,
                           nsIPrincipal* aOriginPrincipal, PRBool aAugmentFlag,
                           nsXBLBinding** aBinding, PRBool* aResolveStyle) 
{
  NS_PRECONDITION(aOriginPrincipal, "Must have an origin principal");
  
  *aBinding = nsnull;
  *aResolveStyle = PR_FALSE;

  nsresult rv;

  nsCOMPtr<nsIDocument> document = aContent->GetOwnerDoc();

  // XXX document may be null if we're in the midst of paint suppression
  if (!document)
    return NS_OK;

  nsCAutoString urlspec;
  if (nsContentUtils::GetWrapperSafeScriptFilename(document, aURL, urlspec)) {
    // Block an attempt to load a binding that has special wrapper
    // automation needs.

    return NS_OK;
  }

  nsBindingManager *bindingManager = document->BindingManager();
  
  nsXBLBinding *binding = bindingManager->GetBinding(aContent);
  if (binding && !aAugmentFlag) {
    nsXBLBinding *styleBinding = binding->GetFirstStyleBinding();
    if (styleBinding) {
      if (binding->MarkedForDeath()) {
        FlushStyleBindings(aContent);
        binding = nsnull;
      }
      else {
        // See if the URIs match.
        nsIURI* uri = styleBinding->PrototypeBinding()->BindingURI();
        PRBool equal;
        if (NS_SUCCEEDED(uri->Equals(aURL, &equal)) && equal)
          return NS_OK;
        FlushStyleBindings(aContent);
        binding = nsnull;
      }
    }
  }

  PRBool ready;
  nsRefPtr<nsXBLBinding> newBinding;
  if (NS_FAILED(rv = GetBinding(aContent, aURL, PR_FALSE, aOriginPrincipal,
                                &ready, getter_AddRefs(newBinding)))) {
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

  if (::IsAncestorBinding(document, aURL, aContent)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (aAugmentFlag) {
    nsXBLBinding *baseBinding;
    nsXBLBinding *nextBinding = newBinding;
    do {
      baseBinding = nextBinding;
      nextBinding = baseBinding->GetBaseBinding();
      baseBinding->SetIsStyleBinding(PR_FALSE);
    } while (nextBinding);

    // XXX Handle adjusting the prototype chain! We need to somehow indicate to
    // InstallImplementation that the whole chain should just be whacked and rebuilt.
    // We are becoming the new binding.
    baseBinding->SetBaseBinding(binding);
    bindingManager->SetBinding(aContent, newBinding);
  }
  else {
    // We loaded a style binding.  It goes on the end.
    if (binding) {
      // Get the last binding that is in the append layer.
      binding->RootBinding()->SetBaseBinding(newBinding);
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
  rv = newBinding->InstallImplementation();
  NS_ENSURE_SUCCESS(rv, rv);

  // Figure out if we have any scoped sheets.  If so, we do a second resolve.
  *aResolveStyle = newBinding->HasStyleSheets();
  
  newBinding.swap(*aBinding);

  return NS_OK; 
}

nsresult
nsXBLService::FlushStyleBindings(nsIContent* aContent)
{
  nsCOMPtr<nsIDocument> document = aContent->GetOwnerDoc();

  // XXX doc will be null if we're in the midst of paint suppression.
  if (! document)
    return NS_OK;

  nsBindingManager *bindingManager = document->BindingManager();
  
  nsXBLBinding *binding = bindingManager->GetBinding(aContent);
  
  if (binding) {
    nsXBLBinding *styleBinding = binding->GetFirstStyleBinding();

    if (styleBinding) {
      // Clear out the script references.
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
  nsIDocument* document = aContent->GetOwnerDoc();
  if (document) {
    *aResult = document->BindingManager()->ResolveTag(aContent, aNameSpaceID);
    NS_IF_ADDREF(*aResult);
  }
  else {
    *aNameSpaceID = aContent->GetNameSpaceID();
    NS_ADDREF(*aResult = aContent->Tag());
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
nsXBLService::AttachGlobalKeyHandler(nsPIDOMEventTarget* aTarget)
{
  // check if the receiver is a content node (not a document), and hook
  // it to the document if that is the case.
  nsCOMPtr<nsPIDOMEventTarget> piTarget = aTarget;
  nsCOMPtr<nsIContent> contentNode(do_QueryInterface(aTarget));
  if (contentNode) {
    // Only attach if we're really in a document
    nsCOMPtr<nsIDocument> doc = contentNode->GetCurrentDoc();
    if (doc)
      piTarget = do_QueryInterface(doc); // We're a XUL keyset. Attach to our document.
  }
    
  if (!piTarget)
    return NS_ERROR_FAILURE;

  // the listener already exists, so skip this
  if (contentNode && contentNode->GetProperty(nsGkAtoms::listener))
    return NS_OK;
    
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(contentNode));

  // Create the key handler
  nsXBLWindowKeyHandler* handler;
  NS_NewXBLWindowKeyHandler(elt, piTarget, &handler); // This addRef's
  if (!handler)
    return NS_ERROR_FAILURE;

  // listen to these events
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  piTarget->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(piTarget);

  target->AddGroupedEventListener(NS_LITERAL_STRING("keydown"), handler,
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("keyup"), handler, 
                                  PR_FALSE, systemGroup);
  target->AddGroupedEventListener(NS_LITERAL_STRING("keypress"), handler, 
                                  PR_FALSE, systemGroup);

  if (contentNode)
    return contentNode->SetProperty(nsGkAtoms::listener, handler,
                                    nsPropertyTable::SupportsDtorFunc, PR_TRUE);

  // release the handler. The reference will be maintained by the event target,
  // and, if there is a content node, the property.
  NS_RELEASE(handler);
  return NS_OK;
}

//
// DetachGlobalKeyHandler
//
// Removes a key handler added by DeatchGlobalKeyHandler.
//
NS_IMETHODIMP
nsXBLService::DetachGlobalKeyHandler(nsPIDOMEventTarget* aTarget)
{
  nsCOMPtr<nsPIDOMEventTarget> piTarget = aTarget;
  nsCOMPtr<nsIContent> contentNode(do_QueryInterface(aTarget));
  if (!contentNode) // detaching is only supported for content nodes
    return NS_ERROR_FAILURE;

  // Only attach if we're really in a document
  nsCOMPtr<nsIDocument> doc = contentNode->GetCurrentDoc();
  if (doc)
    piTarget = do_QueryInterface(doc);
  if (!piTarget)
    return NS_ERROR_FAILURE;

  nsIDOMEventListener* handler =
    static_cast<nsIDOMEventListener*>(contentNode->GetProperty(nsGkAtoms::listener));
  if (!handler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  piTarget->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(piTarget);

  target->RemoveGroupedEventListener(NS_LITERAL_STRING("keydown"), handler,
                                     PR_FALSE, systemGroup);
  target->RemoveGroupedEventListener(NS_LITERAL_STRING("keyup"), handler, 
                                     PR_FALSE, systemGroup);
  target->RemoveGroupedEventListener(NS_LITERAL_STRING("keypress"), handler, 
                                     PR_FALSE, systemGroup);

  contentNode->DeleteProperty(nsGkAtoms::listener);

  return NS_OK;
}

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
    nsXBLJSClass* c = static_cast<nsXBLJSClass*>(lru);

    JS_REMOVE_AND_INIT_LINK(lru);
    delete c;
    gClassLRUListLength--;
  }
  return NS_OK;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsXBLService::BindingReady(nsIContent* aBoundElement, 
                                         nsIURI* aURI, 
                                         PRBool* aIsReady)
{
  // Don't do a security check here; we know this binding is set to go.
  return GetBinding(aBoundElement, aURI, PR_TRUE, nsnull, aIsReady, nsnull);
}

nsresult
nsXBLService::GetBinding(nsIContent* aBoundElement, nsIURI* aURI, 
                         PRBool aPeekOnly, nsIPrincipal* aOriginPrincipal,
                         PRBool* aIsReady, nsXBLBinding** aResult)
{
  // More than 6 binding URIs are rare, see bug 55070 comment 18.
  nsAutoTArray<nsIURI*, 6> uris;
  return GetBinding(aBoundElement, aURI, aPeekOnly, aOriginPrincipal, aIsReady,
                    aResult, uris);
}

nsresult
nsXBLService::GetBinding(nsIContent* aBoundElement, nsIURI* aURI, 
                         PRBool aPeekOnly, nsIPrincipal* aOriginPrincipal,
                         PRBool* aIsReady, nsXBLBinding** aResult,
                         nsTArray<nsIURI*>& aDontExtendURIs)
{
  NS_ASSERTION(aPeekOnly || aResult,
               "Must have non-null out param if not just peeking to see "
               "whether the binding is ready");
  
  if (aResult)
    *aResult = nsnull;

  if (!aURI)
    return NS_ERROR_FAILURE;

  NS_ENSURE_TRUE(aDontExtendURIs.AppendElement(aURI), NS_ERROR_OUT_OF_MEMORY);

  nsCAutoString ref;
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (url)
    url->GetRef(ref);

  nsCOMPtr<nsIDocument> boundDocument = aBoundElement->GetOwnerDoc();

  nsCOMPtr<nsIXBLDocumentInfo> docInfo;
  nsresult rv = LoadBindingDocumentInfo(aBoundElement, boundDocument, aURI,
                                        aOriginPrincipal,
                                        PR_FALSE, getter_AddRefs(docInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  
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
  nsRefPtr<nsXBLBinding> baseBinding;
  PRBool hasBase = protoBinding->HasBasePrototype();
  nsXBLPrototypeBinding* baseProto = protoBinding->GetBasePrototype();
  if (baseProto) {
    // Use the NodePrincipal() of the <binding> element in question
    // for the security check.
    rv = GetBinding(aBoundElement, baseProto->BindingURI(), aPeekOnly,
                    child->NodePrincipal(), aIsReady,
                    getter_AddRefs(baseBinding), aDontExtendURIs);
    if (NS_FAILED(rv))
      return rv; // We aren't ready yet.
  }
  else if (hasBase) {
    // Check for the presence of 'extends' and 'display' attributes
    nsAutoString display, extends;
    child->GetAttr(kNameSpaceID_None, nsGkAtoms::display, display);
    child->GetAttr(kNameSpaceID_None, nsGkAtoms::extends, extends);
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
              //child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::extends, PR_FALSE);
            }

            PRInt32 nameSpaceID =
              nsContentUtils::NameSpaceManager()->GetNameSpaceID(nameSpace);

            nsCOMPtr<nsIAtom> tagName = do_GetAtom(display);
            // Check the white list
            if (!CheckTagNameWhiteList(nameSpaceID, tagName)) {
              const PRUnichar* params[] = { display.get() };
              nsContentUtils::ReportToConsole(nsContentUtils::eXBL_PROPERTIES,
                                              "InvalidExtendsBinding",
                                              params, NS_ARRAY_LENGTH(params),
                                              doc->GetDocumentURI(),
                                              EmptyString(), 0, 0,
                                              nsIScriptError::errorFlag,
                                              "XBL");
              NS_ASSERTION(!IsChromeOrResourceURI(aURI),
                           "Invalid extends value");
              return NS_ERROR_ILLEGAL_VALUE;
            }

            protoBinding->SetBaseTag(nameSpaceID, tagName);
          }
        }
      }

      if (hasExtends && (hasDisplay || nameSpace.IsEmpty())) {
        // Look up the prefix.
        // We have a base class binding. Load it right now.
        nsCOMPtr<nsIURI> bindingURI;
        rv = NS_NewURI(getter_AddRefs(bindingURI), value,
                       doc->GetDocumentCharacterSet().get(),
                       doc->GetBaseURI());
        NS_ENSURE_SUCCESS(rv, rv);
        
        PRUint32 count = aDontExtendURIs.Length();
        for (PRUint32 index = 0; index < count; ++index) {
          PRBool equal;
          rv = aDontExtendURIs[index]->Equals(bindingURI, &equal);
          NS_ENSURE_SUCCESS(rv, rv);
          if (equal) {
            nsCAutoString spec;
            protoBinding->BindingURI()->GetSpec(spec);
            NS_ConvertUTF8toUTF16 protoSpec(spec);
            const PRUnichar* params[] = { protoSpec.get(), value.get() };
            nsContentUtils::ReportToConsole(nsContentUtils::eXBL_PROPERTIES,
                                            "CircularExtendsBinding",
                                            params, NS_ARRAY_LENGTH(params),
                                            boundDocument->GetDocumentURI(),
                                            EmptyString(), 0, 0,
                                            nsIScriptError::warningFlag,
                                            "XBL");
            return NS_ERROR_ILLEGAL_VALUE;
          }
        }

        // Use the NodePrincipal() of the <binding> element in question
        // for the security check.
        rv = GetBinding(aBoundElement, bindingURI, aPeekOnly,
                        child->NodePrincipal(), aIsReady,
                        getter_AddRefs(baseBinding), aDontExtendURIs);
        if (NS_FAILED(rv))
          return rv; // Binding not yet ready or an error occurred.
        if (!aPeekOnly) {
          // Make sure to set the base prototype.
          baseProto = baseBinding->PrototypeBinding();
          protoBinding->SetBasePrototype(baseProto);
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::extends, PR_FALSE);
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::display, PR_FALSE);
        }
      }
    }
  }

  *aIsReady = PR_TRUE;
  if (!aPeekOnly) {
    // Make a new binding
    nsXBLBinding *newBinding = new nsXBLBinding(protoBinding);
    NS_ENSURE_TRUE(newBinding, NS_ERROR_OUT_OF_MEMORY);

    if (baseBinding)
      newBinding->SetBaseBinding(baseBinding);

    NS_ADDREF(*aResult = newBinding);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                      nsIDocument* aBoundDocument,
                                      nsIURI* aBindingURI,
                                      nsIPrincipal* aOriginPrincipal,
                                      PRBool aForceSyncLoad,
                                      nsIXBLDocumentInfo** aResult)
{
  NS_PRECONDITION(aBindingURI, "Must have a binding URI");
  NS_PRECONDITION(!aOriginPrincipal || aBoundDocument,
                  "If we're doing a security check, we better have a document!");
  
  nsresult rv;
  if (aOriginPrincipal) {
    // Security check - Enforce same-origin policy, except to chrome.
    // We have to be careful to not pass aContent as the context here. 
    // Otherwise, if there is a JS-implemented content policy, we will attempt
    // to wrap the content node, which will try to load XBL bindings for it, if
    // any. Since we're not done loading this binding yet, that will reenter
    // this method and we'll end up creating a binding and then immediately
    // clobbering it in our table.  That makes things very confused, leading to
    // misbehavior and crashes.
    rv = nsContentUtils::
      CheckSecurityBeforeLoad(aBindingURI, aOriginPrincipal,
                              nsIScriptSecurityManager::ALLOW_CHROME,
                              gAllowDataURIs,
                              nsIContentPolicy::TYPE_XBL,
                              aBoundDocument);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aResult = nsnull;
  nsCOMPtr<nsIXBLDocumentInfo> info;

  nsCOMPtr<nsIURI> documentURI;
  rv = aBindingURI->Clone(getter_AddRefs(documentURI));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIURL> documentURL(do_QueryInterface(documentURI));
  if (documentURL)
    documentURL->SetRef(EmptyCString());

#ifdef MOZ_XUL
  // We've got a file.  Check our XBL document cache.
  nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
  PRBool useXULCache = cache && cache->IsEnabled(); 

  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so that any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    info = cache->GetXBLDocumentInfo(documentURI); 
  }
#endif

  if (!info) {
    // The second line of defense is the binding manager's document table.
    nsBindingManager *bindingManager = nsnull;

    if (aBoundDocument) {
      bindingManager = aBoundDocument->BindingManager();
      info = bindingManager->GetXBLDocumentInfo(documentURI);
    }

    nsINodeInfo *ni = nsnull;
    if (aBoundElement)
      ni = aBoundElement->NodeInfo();

    if (!info && bindingManager &&
        (!ni || !(ni->Equals(nsGkAtoms::scrollbar, kNameSpaceID_XUL) ||
                  ni->Equals(nsGkAtoms::thumb, kNameSpaceID_XUL) ||
                  ((ni->Equals(nsGkAtoms::input) ||
                    ni->Equals(nsGkAtoms::select)) &&
                   aBoundElement->IsNodeOfType(nsINode::eHTML)))) &&
        !aForceSyncLoad) {
      // The third line of defense is to investigate whether or not the
      // document is currently being loaded asynchronously.  If so, there's no
      // document yet, but we need to glom on our request so that it will be
      // processed whenever the doc does finish loading.
      nsCOMPtr<nsIStreamListener> listener;
      if (bindingManager)
        listener = bindingManager->GetLoadingDocListener(documentURI);
      if (listener) {
        nsXBLStreamListener* xblListener =
          static_cast<nsXBLStreamListener*>(listener.get());
        // Create a new load observer.
        if (!xblListener->HasRequest(aBindingURI, aBoundElement)) {
          nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool, aBindingURI, aBoundElement);
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
                           aBindingURI, aForceSyncLoad, getter_AddRefs(document));
   
      if (document) {
        nsBindingManager *xblDocBindingManager = document->BindingManager();
        info = xblDocBindingManager->GetXBLDocumentInfo(documentURI);
        if (!info) {
          NS_ERROR("An XBL file is malformed.  Did you forget the XBL namespace on the bindings tag?");
          return NS_ERROR_FAILURE;
        }
        xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.

        // If the doc is a chrome URI, then we put it into the XUL cache.
#ifdef MOZ_XUL
        if (useXULCache && IsChromeOrResourceURI(documentURI)) {
          cache->PutXBLDocumentInfo(info);
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
                                   nsIURI* aDocumentURI, nsIURI* aBindingURI, 
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

  // We really shouldn't have to force a sync load for anything here... could
  // we get away with not doing that?  Not sure.
  if (IsChromeOrResourceURI(aDocumentURI))
    aForceSyncLoad = PR_TRUE;

  // Create document and contentsink and set them up.
  nsCOMPtr<nsIDocument> doc;
  rv = NS_NewXMLDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLContentSink> xblSink;
  rv = NS_NewXBLContentSink(getter_AddRefs(xblSink), doc, aDocumentURI, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Open channel
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aDocumentURI, nsnull, loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInterfaceRequestor> sameOriginChecker = nsContentUtils::GetSameOriginChecker();
  NS_ENSURE_TRUE(sameOriginChecker, NS_ERROR_OUT_OF_MEMORY);

  channel->SetNotificationCallbacks(sameOriginChecker);

  nsCOMPtr<nsIStreamListener> listener;
  rv = doc->StartDocumentLoad("loadAsInteractiveData",
                              channel,
                              loadGroup,
                              nsnull,
                              getter_AddRefs(listener),
                              PR_TRUE,
                              xblSink);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aForceSyncLoad) {
    // We can be asynchronous
    nsXBLStreamListener* xblListener = new nsXBLStreamListener(this, listener, aBoundDocument);
    NS_ENSURE_TRUE(xblListener,NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(doc));
    target->AddEventListener(NS_LITERAL_STRING("load"),
                             static_cast<nsIDOMLoadListener*>(xblListener),
                             PR_FALSE);

    // Add ourselves to the list of loading docs.
    nsBindingManager *bindingManager;
    if (aBoundDocument)
      bindingManager = aBoundDocument->BindingManager();
    else
      bindingManager = nsnull;

    if (bindingManager)
      bindingManager->PutLoadingDocListener(aDocumentURI, xblListener);

    // Add our request.
    nsXBLBindingRequest* req = nsXBLBindingRequest::Create(mPool,
                                                           aBindingURI,
                                                           aBoundElement);
    xblListener->AddRequest(req);

    // Now kick off the async read.
    rv = channel->AsyncOpen(xblListener, nsnull);
    if (NS_FAILED(rv)) {
      // Well, we won't be getting a load.  Make sure to clean up our stuff!
      target->RemoveEventListener(NS_LITERAL_STRING("load"),
                                  static_cast<nsIDOMLoadListener*>(xblListener),
                                  PR_FALSE);
      if (bindingManager) {
        bindingManager->RemoveLoadingDocListener(aDocumentURI);
      }
    }
    return NS_OK;
  }

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open(getter_AddRefs(in));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsSyncLoadService::PushSyncStreamToListener(in, listener, channel);
  NS_ENSURE_SUCCESS(rv, rv);

  doc.swap(*aResult);

  return NS_OK;
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

