/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsXBLService.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsIInputStream.h"
#include "nsNameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIDOMElement.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "mozilla/dom/XMLDocument.h"
#include "nsGkAtoms.h"
#include "nsIMemory.h"
#include "nsIObserverService.h"
#include "nsIDOMNodeList.h"
#include "nsXBLContentSink.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLDocumentInfo.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsSyncLoadService.h"
#include "nsContentPolicyUtils.h"
#include "nsTArray.h"
#include "nsError.h"

#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptError.h"
#include "nsXBLSerialize.h"

#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif
#include "nsIDOMEventListener.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

#define NS_MAX_XBL_BINDING_RECURSION 20

nsXBLService* nsXBLService::gInstance = nullptr;

static bool
IsAncestorBinding(nsIDocument* aDocument,
                  nsIURI* aChildBindingURI,
                  nsIContent* aChild)
{
  NS_ASSERTION(aDocument, "expected a document");
  NS_ASSERTION(aChildBindingURI, "expected a binding URI");
  NS_ASSERTION(aChild, "expected a child content");

  uint32_t bindingRecursion = 0;
  for (nsIContent *bindingParent = aChild->GetBindingParent();
       bindingParent;
       bindingParent = bindingParent->GetBindingParent()) {
    nsXBLBinding* binding = bindingParent->GetXBLBinding();
    if (!binding) {
      continue;
    }

    if (binding->PrototypeBinding()->CompareBindingURI(aChildBindingURI)) {
      ++bindingRecursion;
      if (bindingRecursion < NS_MAX_XBL_BINDING_RECURSION) {
        continue;
      }
      nsAutoCString spec;
      aChildBindingURI->GetSpec(spec);
      NS_ConvertUTF8toUTF16 bindingURI(spec);
      const char16_t* params[] = { bindingURI.get() };
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("XBL"), aDocument,
                                      nsContentUtils::eXBL_PROPERTIES,
                                      "TooDeepBindingRecursion",
                                      params, ArrayLength(params));
      return true;
    }
  }

  return false;
}

// Individual binding requests.
class nsXBLBindingRequest
{
public:
  nsCOMPtr<nsIURI> mBindingURI;
  nsCOMPtr<nsIContent> mBoundElement;

  void DocumentLoaded(nsIDocument* aBindingDoc)
  {
    // We only need the document here to cause frame construction, so
    // we need the current doc, not the owner doc.
    nsIDocument* doc = mBoundElement->GetCurrentDoc();
    if (!doc)
      return;

    // Get the binding.
    bool ready = false;
    nsXBLService::GetInstance()->BindingReady(mBoundElement, mBindingURI, &ready);
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
    nsIPresShell *shell = doc->GetShell();
    if (shell) {
      nsIFrame* childFrame = mBoundElement->GetPrimaryFrame();
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

  nsXBLBindingRequest(nsIURI* aURI, nsIContent* aBoundElement)
    : mBindingURI(aURI),
      mBoundElement(aBoundElement)
  {
  }
};

// nsXBLStreamListener, a helper class used for 
// asynchronous parsing of URLs
/* Header file */
class nsXBLStreamListener MOZ_FINAL : public nsIStreamListener,
                                      public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  nsXBLStreamListener(nsIDocument* aBoundDocument,
                      nsIXMLContentSink* aSink,
                      nsIDocument* aBindingDocument);
  ~nsXBLStreamListener();

  void AddRequest(nsXBLBindingRequest* aRequest) { mBindingRequests.AppendElement(aRequest); }
  bool HasRequest(nsIURI* aURI, nsIContent* aBoundElement);

private:
  nsCOMPtr<nsIStreamListener> mInner;
  nsAutoTArray<nsXBLBindingRequest*, 8> mBindingRequests;
  
  nsCOMPtr<nsIWeakReference> mBoundDocument;
  nsCOMPtr<nsIXMLContentSink> mSink; // Only set until OnStartRequest
  nsCOMPtr<nsIDocument> mBindingDocument; // Only set until OnStartRequest
};

/* Implementation file */
NS_IMPL_ISUPPORTS3(nsXBLStreamListener,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIDOMEventListener)

nsXBLStreamListener::nsXBLStreamListener(nsIDocument* aBoundDocument,
                                         nsIXMLContentSink* aSink,
                                         nsIDocument* aBindingDocument)
: mSink(aSink), mBindingDocument(aBindingDocument)
{
  /* member initializers and constructor code */
  mBoundDocument = do_GetWeakReference(aBoundDocument);
}

nsXBLStreamListener::~nsXBLStreamListener()
{
  for (uint32_t i = 0; i < mBindingRequests.Length(); i++) {
    nsXBLBindingRequest* req = mBindingRequests.ElementAt(i);
    delete req;
  }
}

NS_IMETHODIMP
nsXBLStreamListener::OnDataAvailable(nsIRequest *request, nsISupports* aCtxt,
                                     nsIInputStream* aInStr,
                                     uint64_t aSourceOffset, uint32_t aCount)
{
  if (mInner)
    return mInner->OnDataAvailable(request, aCtxt, aInStr, aSourceOffset, aCount);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXBLStreamListener::OnStartRequest(nsIRequest* request, nsISupports* aCtxt)
{
  // Make sure we don't hold on to the sink and binding document past this point
  nsCOMPtr<nsIXMLContentSink> sink;
  mSink.swap(sink);
  nsCOMPtr<nsIDocument> doc;
  mBindingDocument.swap(doc);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsILoadGroup> group;
  request->GetLoadGroup(getter_AddRefs(group));

  nsresult rv = doc->StartDocumentLoad("loadAsInteractiveData",
                                       channel,
                                       group,
                                       nullptr,
                                       getter_AddRefs(mInner),
                                       true,
                                       sink);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure to add ourselves as a listener after StartDocumentLoad,
  // since that resets the event listners on the document.
  doc->AddEventListener(NS_LITERAL_STRING("load"), this, false);

  return mInner->OnStartRequest(request, aCtxt);
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
  mInner = nullptr;

  return rv;
}

bool
nsXBLStreamListener::HasRequest(nsIURI* aURI, nsIContent* aElt)
{
  // XXX Could be more efficient.
  uint32_t count = mBindingRequests.Length();
  for (uint32_t i = 0; i < count; i++) {
    nsXBLBindingRequest* req = mBindingRequests.ElementAt(i);
    bool eq;
    if (req->mBoundElement == aElt &&
        NS_SUCCEEDED(req->mBindingURI->Equals(aURI, &eq)) && eq)
      return true;
  }

  return false;
}

nsresult
nsXBLStreamListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsresult rv = NS_OK;
  uint32_t i;
  uint32_t count = mBindingRequests.Length();

  // Get the binding document; note that we don't hold onto it in this object
  // to avoid creating a cycle
  Event* event = aEvent->InternalDOMEvent();
  EventTarget* target = event->GetCurrentTarget();
  nsCOMPtr<nsIDocument> bindingDocument = do_QueryInterface(target);
  NS_ASSERTION(bindingDocument, "Event not targeted at document?!");

  // See if we're still alive.
  nsCOMPtr<nsIDocument> doc(do_QueryReferent(mBoundDocument));
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
      nsXBLBindingRequest* req = mBindingRequests.ElementAt(0);
      nsIDocument* document = req->mBoundElement->GetCurrentDoc();
      if (document)
        document->FlushPendingNotifications(Flush_ContentAndNotify);
    }

    // Remove ourselves from the set of pending docs.
    nsBindingManager *bindingManager = doc->BindingManager();
    nsIURI* documentURI = bindingDocument->GetDocumentURI();
    bindingManager->RemoveLoadingDocListener(documentURI);

    if (!bindingDocument->GetRootElement()) {
      // FIXME: How about an error console warning?
      NS_WARNING("XBL doc with no root element - this usually shouldn't happen");
      return NS_ERROR_FAILURE;
    }

    // Put our doc info in the doc table.
    nsBindingManager *xblDocBindingManager = bindingDocument->BindingManager();
    nsRefPtr<nsXBLDocumentInfo> info =
      xblDocBindingManager->GetXBLDocumentInfo(documentURI);
    xblDocBindingManager->RemoveXBLDocumentInfo(info); // Break the self-imposed cycle.
    if (!info) {
      if (nsXBLService::IsChromeOrResourceURI(documentURI)) {
        NS_WARNING("An XBL file is malformed. Did you forget the XBL namespace on the bindings tag?");
      }
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("XBL"), nullptr,
                                      nsContentUtils::eXBL_PROPERTIES,
                                      "MalformedXBL",
                                      nullptr, 0, documentURI);
      return NS_ERROR_FAILURE;
    }

    // If the doc is a chrome URI, then we put it into the XUL cache.
#ifdef MOZ_XUL
    if (nsXBLService::IsChromeOrResourceURI(documentURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache && cache->IsEnabled())
        cache->PutXBLDocumentInfo(info);
    }
#endif
  
    bindingManager->PutXBLDocumentInfo(info);

    // Notify all pending requests that their bindings are
    // ready and can be installed.
    for (i = 0; i < count; i++) {
      nsXBLBindingRequest* req = mBindingRequests.ElementAt(i);
      req->DocumentLoaded(bindingDocument);
    }
  }

  target->RemoveEventListener(NS_LITERAL_STRING("load"), this, false);

  return rv;
}

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
bool nsXBLService::gAllowDataURIs = false;

nsHashtable* nsXBLService::gClassTable = nullptr;

LinkedList<nsXBLJSClass>* nsXBLService::gClassLRUList = nullptr;
uint32_t nsXBLService::gClassLRUListLength = 0;
uint32_t nsXBLService::gClassLRUListQuota = 64;

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsXBLService, nsIObserver, nsISupportsWeakReference)

void
nsXBLService::Init()
{
  gInstance = new nsXBLService();
  NS_ADDREF(gInstance);

  // Register the first (and only) nsXBLService as a memory pressure observer
  // so it can flush the LRU list in low-memory situations.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->AddObserver(gInstance, "memory-pressure", true);
}

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  gClassTable = new nsHashtable();
  gClassLRUList = new LinkedList<nsXBLJSClass>();

  Preferences::AddBoolVarCache(&gAllowDataURIs, "layout.debug.enable_data_xbl");
}

nsXBLService::~nsXBLService(void)
{
  // Walk the LRU list removing and deleting the nsXBLJSClasses.
  FlushMemory();

  // Any straggling nsXBLJSClass instances held by unfinalized JS objects
  // created for bindings will be deleted when those objects are finalized
  // (and not put on gClassLRUList, because length >= quota).
  gClassLRUListLength = gClassLRUListQuota = 0;
  delete gClassLRUList;
  gClassLRUList = nullptr;

  // At this point, the only hash table entries should be for referenced
  // XBL class structs held by unfinalized JS binding objects.
  delete gClassTable;
  gClassTable = nullptr;
}

// static
bool
nsXBLService::IsChromeOrResourceURI(nsIURI* aURI)
{
  bool isChrome = false;
  bool isResource = false;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && 
      NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)))
      return (isChrome || isResource);
  return false;
}


// This function loads a particular XBL file and installs all of the bindings
// onto the element.
nsresult
nsXBLService::LoadBindings(nsIContent* aContent, nsIURI* aURL,
                           nsIPrincipal* aOriginPrincipal,
                           nsXBLBinding** aBinding, bool* aResolveStyle) 
{
  NS_PRECONDITION(aOriginPrincipal, "Must have an origin principal");
  
  *aBinding = nullptr;
  *aResolveStyle = false;

  nsresult rv;

  nsCOMPtr<nsIDocument> document = aContent->OwnerDoc();

  nsAutoCString urlspec;
  if (nsContentUtils::GetWrapperSafeScriptFilename(document, aURL, urlspec)) {
    // Block an attempt to load a binding that has special wrapper
    // automation needs.

    return NS_OK;
  }

  nsXBLBinding *binding = aContent->GetXBLBinding();
  if (binding) {
    if (binding->MarkedForDeath()) {
      FlushStyleBindings(aContent);
      binding = nullptr;
    }
    else {
      // See if the URIs match.
      if (binding->PrototypeBinding()->CompareBindingURI(aURL))
        return NS_OK;
      FlushStyleBindings(aContent);
      binding = nullptr;
    }
  }

  bool ready;
  nsRefPtr<nsXBLBinding> newBinding;
  if (NS_FAILED(rv = GetBinding(aContent, aURL, false, aOriginPrincipal,
                                &ready, getter_AddRefs(newBinding)))) {
    return rv;
  }

  if (!newBinding) {
#ifdef DEBUG
    nsAutoCString spec;
    aURL->GetSpec(spec);
    nsAutoCString str(NS_LITERAL_CSTRING("Failed to locate XBL binding. XBL is now using id instead of name to reference bindings. Make sure you have switched over.  The invalid binding name is: ") + spec);
    NS_ERROR(str.get());
#endif
    return NS_OK;
  }

  if (::IsAncestorBinding(document, aURL, aContent)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // We loaded a style binding.  It goes on the end.
  if (binding) {
    // Get the last binding that is in the append layer.
    binding->RootBinding()->SetBaseBinding(newBinding);
  }
  else {
    // Install the binding on the content node.
    aContent->SetXBLBinding(newBinding);
  }

  {
    nsAutoScriptBlocker scriptBlocker;

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
  }

  return NS_OK; 
}

nsresult
nsXBLService::FlushStyleBindings(nsIContent* aContent)
{
  nsCOMPtr<nsIDocument> document = aContent->OwnerDoc();

  nsXBLBinding *binding = aContent->GetXBLBinding();
  if (binding) {
    // Clear out the script references.
    binding->ChangeDocument(document, nullptr);

    aContent->SetXBLBinding(nullptr); // Flush old style bindings
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
nsresult
nsXBLService::AttachGlobalKeyHandler(EventTarget* aTarget)
{
  // check if the receiver is a content node (not a document), and hook
  // it to the document if that is the case.
  nsCOMPtr<EventTarget> piTarget = aTarget;
  nsCOMPtr<nsIContent> contentNode(do_QueryInterface(aTarget));
  if (contentNode) {
    // Only attach if we're really in a document
    nsCOMPtr<nsIDocument> doc = contentNode->GetCurrentDoc();
    if (doc)
      piTarget = doc; // We're a XUL keyset. Attach to our document.
  }

  EventListenerManager* manager = piTarget->GetOrCreateListenerManager();

  if (!piTarget || !manager)
    return NS_ERROR_FAILURE;

  // the listener already exists, so skip this
  if (contentNode && contentNode->GetProperty(nsGkAtoms::listener))
    return NS_OK;

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(contentNode));

  // Create the key handler
  nsRefPtr<nsXBLWindowKeyHandler> handler =
    NS_NewXBLWindowKeyHandler(elt, piTarget);

  // listen to these events
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keydown"),
                                  TrustedEventsAtSystemGroupBubble());
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keyup"),
                                  TrustedEventsAtSystemGroupBubble());
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keypress"),
                                  TrustedEventsAtSystemGroupBubble());

  // The capturing listener is only used for XUL keysets to properly handle
  // shortcut keys in a multi-process environment.
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keydown"),
                                  TrustedEventsAtSystemGroupCapture());
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keyup"),
                                  TrustedEventsAtSystemGroupCapture());
  manager->AddEventListenerByType(handler, NS_LITERAL_STRING("keypress"),
                                  TrustedEventsAtSystemGroupCapture());

  if (contentNode)
    return contentNode->SetProperty(nsGkAtoms::listener,
                                    handler.forget().take(),
                                    nsPropertyTable::SupportsDtorFunc, true);

  // The reference to the handler will be maintained by the event target,
  // and, if there is a content node, the property.
  return NS_OK;
}

//
// DetachGlobalKeyHandler
//
// Removes a key handler added by DeatchGlobalKeyHandler.
//
nsresult
nsXBLService::DetachGlobalKeyHandler(EventTarget* aTarget)
{
  nsCOMPtr<EventTarget> piTarget = aTarget;
  nsCOMPtr<nsIContent> contentNode(do_QueryInterface(aTarget));
  if (!contentNode) // detaching is only supported for content nodes
    return NS_ERROR_FAILURE;

  // Only attach if we're really in a document
  nsCOMPtr<nsIDocument> doc = contentNode->GetCurrentDoc();
  if (doc)
    piTarget = do_QueryInterface(doc);

  EventListenerManager* manager = piTarget->GetOrCreateListenerManager();

  if (!piTarget || !manager)
    return NS_ERROR_FAILURE;

  nsIDOMEventListener* handler =
    static_cast<nsIDOMEventListener*>(contentNode->GetProperty(nsGkAtoms::listener));
  if (!handler)
    return NS_ERROR_FAILURE;

  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keydown"),
                                     TrustedEventsAtSystemGroupBubble());
  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keyup"),
                                     TrustedEventsAtSystemGroupBubble());
  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keypress"),
                                     TrustedEventsAtSystemGroupBubble());

  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keydown"),
                                     TrustedEventsAtSystemGroupCapture());
  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keyup"),
                                     TrustedEventsAtSystemGroupCapture());
  manager->RemoveEventListenerByType(handler, NS_LITERAL_STRING("keypress"),
                                     TrustedEventsAtSystemGroupCapture());

  contentNode->DeleteProperty(nsGkAtoms::listener);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aSomeData)
{
  if (nsCRT::strcmp(aTopic, "memory-pressure") == 0)
    FlushMemory();

  return NS_OK;
}

nsresult
nsXBLService::FlushMemory()
{
  while (!gClassLRUList->isEmpty()) {
    nsXBLJSClass* c = gClassLRUList->popFirst();
    delete c;
    gClassLRUListLength--;
  }
  return NS_OK;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

nsresult
nsXBLService::BindingReady(nsIContent* aBoundElement,
                           nsIURI* aURI, 
                           bool* aIsReady)
{
  // Don't do a security check here; we know this binding is set to go.
  return GetBinding(aBoundElement, aURI, true, nullptr, aIsReady, nullptr);
}

nsresult
nsXBLService::GetBinding(nsIContent* aBoundElement, nsIURI* aURI, 
                         bool aPeekOnly, nsIPrincipal* aOriginPrincipal,
                         bool* aIsReady, nsXBLBinding** aResult)
{
  // More than 6 binding URIs are rare, see bug 55070 comment 18.
  nsAutoTArray<nsIURI*, 6> uris;
  return GetBinding(aBoundElement, aURI, aPeekOnly, aOriginPrincipal, aIsReady,
                    aResult, uris);
}

nsresult
nsXBLService::GetBinding(nsIContent* aBoundElement, nsIURI* aURI, 
                         bool aPeekOnly, nsIPrincipal* aOriginPrincipal,
                         bool* aIsReady, nsXBLBinding** aResult,
                         nsTArray<nsIURI*>& aDontExtendURIs)
{
  NS_ASSERTION(aPeekOnly || aResult,
               "Must have non-null out param if not just peeking to see "
               "whether the binding is ready");
  
  if (aResult)
    *aResult = nullptr;

  if (!aURI)
    return NS_ERROR_FAILURE;

  nsAutoCString ref;
  aURI->GetRef(ref);

  nsCOMPtr<nsIDocument> boundDocument = aBoundElement->OwnerDoc();

  nsRefPtr<nsXBLDocumentInfo> docInfo;
  nsresult rv = LoadBindingDocumentInfo(aBoundElement, boundDocument, aURI,
                                        aOriginPrincipal,
                                        false, getter_AddRefs(docInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!docInfo)
    return NS_ERROR_FAILURE;

  nsXBLPrototypeBinding* protoBinding = docInfo->GetPrototypeBinding(ref);

  if (!protoBinding) {
#ifdef DEBUG
    nsAutoCString uriSpec;
    aURI->GetSpec(uriSpec);
    nsAutoCString doc;
    boundDocument->GetDocumentURI()->GetSpec(doc);
    nsAutoCString message("Unable to locate an XBL binding for URI ");
    message += uriSpec;
    message += " in document ";
    message += doc;
    NS_WARNING(message.get());
#endif
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_TRUE(aDontExtendURIs.AppendElement(protoBinding->BindingURI()),
                 NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr<nsIURI> altBindingURI = protoBinding->AlternateBindingURI();
  if (altBindingURI) {
    NS_ENSURE_TRUE(aDontExtendURIs.AppendElement(altBindingURI),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  // Our prototype binding must have all its resources loaded.
  bool ready = protoBinding->LoadResources();
  if (!ready) {
    // Add our bound element to the protos list of elts that should
    // be notified when the stylesheets and scripts finish loading.
    protoBinding->AddResourceListener(aBoundElement);
    return NS_ERROR_FAILURE; // The binding isn't ready yet.
  }

  rv = protoBinding->ResolveBaseBinding();
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI* baseBindingURI;
  nsXBLPrototypeBinding* baseProto = protoBinding->GetBasePrototype();
  if (baseProto) {
    baseBindingURI = baseProto->BindingURI();
  }
  else {
    baseBindingURI = protoBinding->GetBaseBindingURI();
    if (baseBindingURI) {
      uint32_t count = aDontExtendURIs.Length();
      for (uint32_t index = 0; index < count; ++index) {
        bool equal;
        rv = aDontExtendURIs[index]->Equals(baseBindingURI, &equal);
        NS_ENSURE_SUCCESS(rv, rv);
        if (equal) {
          nsAutoCString spec, basespec;
          protoBinding->BindingURI()->GetSpec(spec);
          NS_ConvertUTF8toUTF16 protoSpec(spec);
          baseBindingURI->GetSpec(basespec);
          NS_ConvertUTF8toUTF16 baseSpecUTF16(basespec);
          const char16_t* params[] = { protoSpec.get(), baseSpecUTF16.get() };
          nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                          NS_LITERAL_CSTRING("XBL"), nullptr,
                                          nsContentUtils::eXBL_PROPERTIES,
                                          "CircularExtendsBinding",
                                          params, ArrayLength(params),
                                          boundDocument->GetDocumentURI());
          return NS_ERROR_ILLEGAL_VALUE;
        }
      }
    }
  }

  nsRefPtr<nsXBLBinding> baseBinding;
  if (baseBindingURI) {
    nsIContent* child = protoBinding->GetBindingElement();
    rv = GetBinding(aBoundElement, baseBindingURI, aPeekOnly,
                    child->NodePrincipal(), aIsReady,
                    getter_AddRefs(baseBinding), aDontExtendURIs);
    if (NS_FAILED(rv))
      return rv; // We aren't ready yet.
  }

  *aIsReady = true;

  if (!aPeekOnly) {
    // Make a new binding
    nsXBLBinding *newBinding = new nsXBLBinding(protoBinding);
    NS_ENSURE_TRUE(newBinding, NS_ERROR_OUT_OF_MEMORY);

    if (baseBinding) {
      if (!baseProto) {
        protoBinding->SetBasePrototype(baseBinding->PrototypeBinding());
      }
       newBinding->SetBaseBinding(baseBinding);
    }

    NS_ADDREF(*aResult = newBinding);
  }

  return NS_OK;
}

static bool SchemeIs(nsIURI* aURI, const char* aScheme)
{
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(baseURI, false);

  bool isScheme = false;
  return NS_SUCCEEDED(baseURI->SchemeIs(aScheme, &isScheme)) && isScheme;
}

static bool
IsSystemOrChromeURLPrincipal(nsIPrincipal* aPrincipal)
{
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    return true;
  }
  
  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, false);
  
  bool isChrome = false;
  return NS_SUCCEEDED(uri->SchemeIs("chrome", &isChrome)) && isChrome;
}

nsresult
nsXBLService::LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                      nsIDocument* aBoundDocument,
                                      nsIURI* aBindingURI,
                                      nsIPrincipal* aOriginPrincipal,
                                      bool aForceSyncLoad,
                                      nsXBLDocumentInfo** aResult)
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
    NS_ENSURE_SUCCESS(rv, NS_ERROR_XBL_BLOCKED);

    if (!IsSystemOrChromeURLPrincipal(aOriginPrincipal)) {
      // Also make sure that we're same-origin with the bound document
      // except if the stylesheet has the system principal.
      if (!(gAllowDataURIs && SchemeIs(aBindingURI, "data")) &&
          !SchemeIs(aBindingURI, "chrome")) {
        rv = aBoundDocument->NodePrincipal()->CheckMayLoad(aBindingURI,
                                                           true, false);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_XBL_BLOCKED);
      }

      // Finally check if this document is allowed to use XBL at all.
      NS_ENSURE_TRUE(aBoundDocument->AllowXULXBL(),
                     NS_ERROR_XBL_BLOCKED);
    }
  }

  *aResult = nullptr;
  nsRefPtr<nsXBLDocumentInfo> info;

  nsCOMPtr<nsIURI> documentURI;
  rv = aBindingURI->CloneIgnoringRef(getter_AddRefs(documentURI));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_XUL
  // We've got a file.  Check our XBL document cache.
  nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
  bool useXULCache = cache && cache->IsEnabled(); 

  if (useXULCache) {
    // The first line of defense is the chrome cache.  
    // This cache crosses the entire product, so that any XBL bindings that are
    // part of chrome will be reused across all XUL documents.
    info = cache->GetXBLDocumentInfo(documentURI); 
  }
#endif

  if (!info) {
    // The second line of defense is the binding manager's document table.
    nsBindingManager *bindingManager = nullptr;

    if (aBoundDocument) {
      bindingManager = aBoundDocument->BindingManager();
      info = bindingManager->GetXBLDocumentInfo(documentURI);
      if (aBoundDocument->IsStaticDocument() &&
          IsChromeOrResourceURI(aBindingURI)) {
        aForceSyncLoad = true;
      }
    }

    nsINodeInfo *ni = nullptr;
    if (aBoundElement)
      ni = aBoundElement->NodeInfo();

    if (!info && bindingManager &&
        (!ni || !(ni->Equals(nsGkAtoms::scrollbar, kNameSpaceID_XUL) ||
                  ni->Equals(nsGkAtoms::thumb, kNameSpaceID_XUL) ||
                  ((ni->Equals(nsGkAtoms::input) ||
                    ni->Equals(nsGkAtoms::select)) &&
                   aBoundElement->IsHTML()))) && !aForceSyncLoad) {
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
          nsXBLBindingRequest* req = new nsXBLBindingRequest(aBindingURI, aBoundElement);
          xblListener->AddRequest(req);
        }
        return NS_OK;
      }
    }

#ifdef MOZ_XUL
    // Next, look in the startup cache
    bool useStartupCache = useXULCache && IsChromeOrResourceURI(documentURI);
    if (!info && useStartupCache) {
      rv = nsXBLDocumentInfo::ReadPrototypeBindings(documentURI, getter_AddRefs(info));
      if (NS_SUCCEEDED(rv)) {
        cache->PutXBLDocumentInfo(info);

        if (bindingManager) {
          // Cache it in our binding manager's document table.
          bindingManager->PutXBLDocumentInfo(info);
        }
      }
    }
#endif

    if (!info) {
      // Finally, if all lines of defense fail, we go and fetch the binding
      // document.
      
      // Always load chrome synchronously
      bool chrome;
      if (NS_SUCCEEDED(documentURI->SchemeIs("chrome", &chrome)) && chrome)
        aForceSyncLoad = true;

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
        if (useStartupCache) {
          cache->PutXBLDocumentInfo(info);

          // now write the bindings into the startup cache
          info->WritePrototypeBindings();
        }
#endif
        
        if (bindingManager) {
          // Also put it in our binding manager's document table.
          bindingManager->PutXBLDocumentInfo(info);
        }
      }
    }
  }

  info.forget(aResult);

  return NS_OK;
}

nsresult
nsXBLService::FetchBindingDocument(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                   nsIURI* aDocumentURI, nsIURI* aBindingURI, 
                                   bool aForceSyncLoad, nsIDocument** aResult)
{
  nsresult rv = NS_OK;
  // Initialize our out pointer to nullptr
  *aResult = nullptr;

  // Now we have to synchronously load the binding file.
  // Create an XML content sink and a parser. 
  nsCOMPtr<nsILoadGroup> loadGroup;
  if (aBoundDocument)
    loadGroup = aBoundDocument->GetDocumentLoadGroup();

  // We really shouldn't have to force a sync load for anything here... could
  // we get away with not doing that?  Not sure.
  if (IsChromeOrResourceURI(aDocumentURI))
    aForceSyncLoad = true;

  // Create document and contentsink and set them up.
  nsCOMPtr<nsIDocument> doc;
  rv = NS_NewXMLDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLContentSink> xblSink;
  rv = NS_NewXBLContentSink(getter_AddRefs(xblSink), doc, aDocumentURI, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // Open channel
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aDocumentURI, nullptr, loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInterfaceRequestor> sameOriginChecker = nsContentUtils::GetSameOriginChecker();
  NS_ENSURE_TRUE(sameOriginChecker, NS_ERROR_OUT_OF_MEMORY);

  channel->SetNotificationCallbacks(sameOriginChecker);

  if (!aForceSyncLoad) {
    // We can be asynchronous
    nsXBLStreamListener* xblListener =
      new nsXBLStreamListener(aBoundDocument, xblSink, doc);
    NS_ENSURE_TRUE(xblListener,NS_ERROR_OUT_OF_MEMORY);

    // Add ourselves to the list of loading docs.
    nsBindingManager *bindingManager;
    if (aBoundDocument)
      bindingManager = aBoundDocument->BindingManager();
    else
      bindingManager = nullptr;

    if (bindingManager)
      bindingManager->PutLoadingDocListener(aDocumentURI, xblListener);

    // Add our request.
    nsXBLBindingRequest* req = new nsXBLBindingRequest(aBindingURI,
                                                       aBoundElement);
    xblListener->AddRequest(req);

    // Now kick off the async read.
    rv = channel->AsyncOpen(xblListener, nullptr);
    if (NS_FAILED(rv)) {
      // Well, we won't be getting a load.  Make sure to clean up our stuff!
      if (bindingManager) {
        bindingManager->RemoveLoadingDocListener(aDocumentURI);
      }
    }
    return NS_OK;
  }

  nsCOMPtr<nsIStreamListener> listener;
  rv = doc->StartDocumentLoad("loadAsInteractiveData",
                              channel,
                              loadGroup,
                              nullptr,
                              getter_AddRefs(listener),
                              true,
                              xblSink);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open(getter_AddRefs(in));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsSyncLoadService::PushSyncStreamToListener(in, listener, channel);
  NS_ENSURE_SUCCESS(rv, rv);

  doc.swap(*aResult);

  return NS_OK;
}

