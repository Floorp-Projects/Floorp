/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocManager.h"

#include "ApplicationAccessible.h"
#include "ARIAMap.h"
#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "DocAccessibleParent.h"
#include "nsAccessibilityService.h"
#include "Platform.h"
#include "RootAccessibleWrap.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "mozilla/Components.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Event.h"  // for Event
#include "nsContentUtils.h"
#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebNavigation.h"
#include "nsServiceManagerUtils.h"
#include "nsIWebProgress.h"
#include "nsCoreUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/BrowserChild.h"
#include "xpcAccessibleDocument.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

StaticAutoPtr<nsTArray<DocAccessibleParent*>> DocManager::sRemoteDocuments;
StaticAutoPtr<nsRefPtrHashtable<nsPtrHashKey<const DocAccessibleParent>,
                                xpcAccessibleDocument>>
    DocManager::sRemoteXPCDocumentCache;

////////////////////////////////////////////////////////////////////////////////
// DocManager
////////////////////////////////////////////////////////////////////////////////

DocManager::DocManager() : mDocAccessibleCache(2), mXPCDocumentCache(0) {}

////////////////////////////////////////////////////////////////////////////////
// DocManager public

DocAccessible* DocManager::GetDocAccessible(Document* aDocument) {
  if (!aDocument) return nullptr;

  DocAccessible* docAcc = GetExistingDocAccessible(aDocument);
  if (docAcc) return docAcc;

  return CreateDocOrRootAccessible(aDocument);
}

DocAccessible* DocManager::GetDocAccessible(const PresShell* aPresShell) {
  if (!aPresShell) {
    return nullptr;
  }

  DocAccessible* doc = aPresShell->GetDocAccessible();
  if (doc) {
    return doc;
  }

  return GetDocAccessible(aPresShell->GetDocument());
}

LocalAccessible* DocManager::FindAccessibleInCache(nsINode* aNode) const {
  for (const auto& docAccessible : mDocAccessibleCache.Values()) {
    NS_ASSERTION(docAccessible,
                 "No doc accessible for the object in doc accessible cache!");

    if (docAccessible) {
      LocalAccessible* accessible = docAccessible->GetAccessible(aNode);
      if (accessible) {
        return accessible;
      }
    }
  }
  return nullptr;
}

void DocManager::RemoveFromXPCDocumentCache(DocAccessible* aDocument,
                                            bool aAllowServiceShutdown) {
  xpcAccessibleDocument* xpcDoc = mXPCDocumentCache.GetWeak(aDocument);
  if (xpcDoc) {
    xpcDoc->Shutdown();
    mXPCDocumentCache.Remove(aDocument);

    if (aAllowServiceShutdown && !HasXPCDocuments()) {
      MaybeShutdownAccService(nsAccessibilityService::eXPCOM);
    }
  }
}

void DocManager::NotifyOfDocumentShutdown(DocAccessible* aDocument,
                                          Document* aDOMDocument,
                                          bool aAllowServiceShutdown) {
  // We need to remove listeners in both cases, when document is being shutdown
  // or when accessibility service is being shut down as well.
  RemoveListeners(aDOMDocument);

  // Document will already be removed when accessibility service is shutting
  // down so we do not need to remove it twice.
  if (nsAccessibilityService::IsShutdown()) {
    return;
  }

  RemoveFromXPCDocumentCache(aDocument, aAllowServiceShutdown);
  mDocAccessibleCache.Remove(aDOMDocument);
}

void DocManager::RemoveFromRemoteXPCDocumentCache(DocAccessibleParent* aDoc) {
  xpcAccessibleDocument* doc = GetCachedXPCDocument(aDoc);
  if (doc) {
    doc->Shutdown();
    sRemoteXPCDocumentCache->Remove(aDoc);
  }

  if (sRemoteXPCDocumentCache && sRemoteXPCDocumentCache->Count() == 0) {
    MaybeShutdownAccService(nsAccessibilityService::eXPCOM);
  }
}

void DocManager::NotifyOfRemoteDocShutdown(DocAccessibleParent* aDoc) {
  RemoveFromRemoteXPCDocumentCache(aDoc);
}

xpcAccessibleDocument* DocManager::GetXPCDocument(DocAccessible* aDocument) {
  if (!aDocument) return nullptr;

  return mXPCDocumentCache.GetOrInsertNew(aDocument, aDocument);
}

xpcAccessibleDocument* DocManager::GetXPCDocument(DocAccessibleParent* aDoc) {
  xpcAccessibleDocument* doc = GetCachedXPCDocument(aDoc);
  if (doc) {
    return doc;
  }

  if (!sRemoteXPCDocumentCache) {
    sRemoteXPCDocumentCache =
        new nsRefPtrHashtable<nsPtrHashKey<const DocAccessibleParent>,
                              xpcAccessibleDocument>;
    ClearOnShutdown(&sRemoteXPCDocumentCache);
  }

  MOZ_ASSERT(!aDoc->IsShutdown(), "Adding a shutdown doc to remote XPC cache");
  doc = new xpcAccessibleDocument(aDoc);
  sRemoteXPCDocumentCache->InsertOrUpdate(aDoc, RefPtr{doc});

  return doc;
}

#ifdef DEBUG
bool DocManager::IsProcessingRefreshDriverNotification() const {
  for (const auto& entry : mDocAccessibleCache) {
    DocAccessible* docAccessible = entry.GetWeak();
    NS_ASSERTION(docAccessible,
                 "No doc accessible for the object in doc accessible cache!");

    if (docAccessible && docAccessible->mNotificationController &&
        docAccessible->mNotificationController->IsUpdating()) {
      return true;
    }
  }
  return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// DocManager protected

bool DocManager::Init() {
  nsCOMPtr<nsIWebProgress> progress = components::DocLoader::Service();

  if (!progress) return false;

  progress->AddProgressListener(static_cast<nsIWebProgressListener*>(this),
                                nsIWebProgress::NOTIFY_STATE_DOCUMENT);

  return true;
}

void DocManager::Shutdown() {
  nsCOMPtr<nsIWebProgress> progress = components::DocLoader::Service();

  if (progress) {
    progress->RemoveProgressListener(
        static_cast<nsIWebProgressListener*>(this));
  }

  ClearDocCache();
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS(DocManager, nsIWebProgressListener, nsIDOMEventListener,
                  nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
DocManager::OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                          uint32_t aStateFlags, nsresult aStatus) {
  NS_ASSERTION(aStateFlags & STATE_IS_DOCUMENT, "Other notifications excluded");

  if (nsAccessibilityService::IsShutdown() || !aWebProgress ||
      (aStateFlags & (STATE_START | STATE_STOP)) == 0) {
    return NS_OK;
  }

  nsCOMPtr<mozIDOMWindowProxy> DOMWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(DOMWindow));
  NS_ENSURE_STATE(DOMWindow);

  nsPIDOMWindowOuter* piWindow = nsPIDOMWindowOuter::From(DOMWindow);
  MOZ_ASSERT(piWindow);

  nsCOMPtr<Document> document = piWindow->GetDoc();
  NS_ENSURE_STATE(document);

  // Document was loaded.
  if (aStateFlags & STATE_STOP) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocLoad)) {
      logging::DocLoad("document loaded", aWebProgress, aRequest, aStateFlags);
    }
#endif

    // Figure out an event type to notify the document has been loaded.
    uint32_t eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED;

    // Some XUL documents get start state and then stop state with failure
    // status when everything is ok. Fire document load complete event in this
    // case.
    if (NS_SUCCEEDED(aStatus) || !document->IsContentDocument()) {
      eventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE;
    }

    // If end consumer has been retargeted for loaded content then do not fire
    // any event because it means no new document has been loaded, for example,
    // it happens when user clicks on file link.
    if (aRequest) {
      uint32_t loadFlags = 0;
      aRequest->GetLoadFlags(&loadFlags);
      if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) eventType = 0;
    }

    HandleDOMDocumentLoad(document, eventType);
    return NS_OK;
  }

  // Document loading was started.
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocLoad)) {
    logging::DocLoad("start document loading", aWebProgress, aRequest,
                     aStateFlags);
  }
#endif

  DocAccessible* docAcc = GetExistingDocAccessible(document);
  if (!docAcc) return NS_OK;

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(DOMWindow));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  NS_ENSURE_STATE(docShell);

  bool isReloading = false;
  uint32_t loadType;
  docShell->GetLoadType(&loadType);
  if (loadType == LOAD_RELOAD_NORMAL || loadType == LOAD_RELOAD_BYPASS_CACHE ||
      loadType == LOAD_RELOAD_BYPASS_PROXY ||
      loadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE) {
    isReloading = true;
  }

  docAcc->NotifyOfLoading(isReloading);
  return NS_OK;
}

NS_IMETHODIMP
DocManager::OnProgressChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                             int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                             int32_t aCurTotalProgress,
                             int32_t aMaxTotalProgress) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
DocManager::OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                             nsIURI* aLocation, uint32_t aFlags) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
DocManager::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                           nsresult aStatus, const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
DocManager::OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                             uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
DocManager::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                   nsIRequest* aRequest, uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
DocManager::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  nsCOMPtr<Document> document = do_QueryInterface(aEvent->GetTarget());
  NS_ASSERTION(document, "pagehide or DOMContentLoaded for non document!");
  if (!document) return NS_OK;

  if (type.EqualsLiteral("pagehide")) {
    // 'pagehide' event is registered on every DOM document we create an
    // accessible for, process the event for the target. This document
    // accessible and all its sub document accessible are shutdown as result of
    // processing.

#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocDestroy)) {
      logging::DocDestroy("received 'pagehide' event", document);
    }
#endif

    // Shutdown this one and sub document accessibles.

    // We're allowed to not remove listeners when accessible document is
    // shutdown since we don't keep strong reference on chrome event target and
    // listeners are removed automatically when chrome event target goes away.
    DocAccessible* docAccessible = GetExistingDocAccessible(document);
    if (docAccessible) docAccessible->Shutdown();

    return NS_OK;
  }

  // XXX: handle error pages loading separately since they get neither
  // webprogress notifications nor 'pageshow' event.
  if (type.EqualsLiteral("DOMContentLoaded") &&
      nsCoreUtils::IsErrorPage(document)) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocLoad)) {
      logging::DocLoad("handled 'DOMContentLoaded' event", document);
    }
#endif

    HandleDOMDocumentLoad(document,
                          nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// DocManager private

void DocManager::HandleDOMDocumentLoad(Document* aDocument,
                                       uint32_t aLoadEventType) {
  // Document accessible can be created before we were notified the DOM document
  // was loaded completely. However if it's not created yet then create it.
  DocAccessible* docAcc = GetExistingDocAccessible(aDocument);
  if (!docAcc) {
    docAcc = CreateDocOrRootAccessible(aDocument);
    if (!docAcc) return;
  }

  docAcc->NotifyOfLoad(aLoadEventType);
}

void DocManager::AddListeners(Document* aDocument,
                              bool aAddDOMContentLoadedListener) {
  nsPIDOMWindowOuter* window = aDocument->GetWindow();
  EventTarget* target = window->GetChromeEventHandler();
  EventListenerManager* elm = target->GetOrCreateListenerManager();
  elm->AddEventListenerByType(this, u"pagehide"_ns, TrustedEventsAtCapture());

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::Text("added 'pagehide' listener");
  }
#endif

  if (aAddDOMContentLoadedListener) {
    elm->AddEventListenerByType(this, u"DOMContentLoaded"_ns,
                                TrustedEventsAtCapture());
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocCreate)) {
      logging::Text("added 'DOMContentLoaded' listener");
    }
#endif
  }
}

void DocManager::RemoveListeners(Document* aDocument) {
  nsPIDOMWindowOuter* window = aDocument->GetWindow();
  if (!window) return;

  EventTarget* target = window->GetChromeEventHandler();
  if (!target) return;

  EventListenerManager* elm = target->GetOrCreateListenerManager();
  elm->RemoveEventListenerByType(this, u"pagehide"_ns,
                                 TrustedEventsAtCapture());

  elm->RemoveEventListenerByType(this, u"DOMContentLoaded"_ns,
                                 TrustedEventsAtCapture());
}

DocAccessible* DocManager::CreateDocOrRootAccessible(Document* aDocument) {
  // Ignore hidden documents, resource documents, static clone
  // (printing) documents and documents without a docshell.
  if (!nsCoreUtils::IsDocumentVisibleConsideringInProcessAncestors(aDocument) ||
      aDocument->IsResourceDoc() || aDocument->IsStaticDocument() ||
      !aDocument->IsActive()) {
    return nullptr;
  }

  nsIDocShell* docShell = aDocument->GetDocShell();
  if (!docShell || docShell->IsInvisible()) {
    return nullptr;
  }

  nsIWidget* widget = nsContentUtils::WidgetForDocument(aDocument);
  if (!widget || widget->WindowType() == eWindowType_invisible) {
    return nullptr;
  }

  // Ignore documents without presshell. We must not ignore documents with no
  // root frame because DOM focus can hit such documents and ignoring them would
  // prevent a11y focus.
  PresShell* presShell = aDocument->GetPresShell();
  if (!presShell || presShell->IsDestroying()) {
    return nullptr;
  }

  bool isRootDoc = nsCoreUtils::IsRootDocument(aDocument);

  DocAccessible* parentDocAcc = nullptr;
  if (!isRootDoc) {
    // XXXaaronl: ideally we would traverse the presshell chain. Since there's
    // no easy way to do that, we cheat and use the document hierarchy.
    parentDocAcc = GetDocAccessible(aDocument->GetInProcessParentDocument());
    NS_ASSERTION(parentDocAcc, "Can't create an accessible for the document!");
    if (!parentDocAcc) return nullptr;
  }

  // We only create root accessibles for the true root, otherwise create a
  // doc accessible.
  RefPtr<DocAccessible> docAcc =
      isRootDoc ? new RootAccessibleWrap(aDocument, presShell)
                : new DocAccessibleWrap(aDocument, presShell);

  // Cache the document accessible into document cache.
  mDocAccessibleCache.InsertOrUpdate(aDocument, RefPtr{docAcc});

  // Initialize the document accessible.
  docAcc->Init();

  // Bind the document to the tree.
  if (isRootDoc) {
    if (!ApplicationAcc()->AppendChild(docAcc)) {
      docAcc->Shutdown();
      return nullptr;
    }

    // Fire reorder event to notify new accessible document has been attached to
    // the tree. The reorder event is delivered after the document tree is
    // constructed because event processing and tree construction are done by
    // the same document.
    // Note: don't use AccReorderEvent to avoid coalsecense and special reorder
    // events processing.
    docAcc->FireDelayedEvent(nsIAccessibleEvent::EVENT_REORDER,
                             ApplicationAcc());

  } else {
    parentDocAcc->BindChildDocument(docAcc);
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::DocCreate("document creation finished", aDocument);
    logging::Stack();
  }
#endif

  AddListeners(aDocument, isRootDoc);
  return docAcc;
}

////////////////////////////////////////////////////////////////////////////////
// DocManager static

void DocManager::ClearDocCache() {
  while (mDocAccessibleCache.Count() > 0) {
    auto iter = mDocAccessibleCache.Iter();
    MOZ_ASSERT(!iter.Done());
    DocAccessible* docAcc = iter.UserData();
    NS_ASSERTION(docAcc,
                 "No doc accessible for the object in doc accessible cache!");
    if (docAcc) {
      docAcc->Shutdown();
    }

    iter.Remove();
  }

  // Ensure that all xpcom accessible documents are shut down as well.
  while (mXPCDocumentCache.Count() > 0) {
    auto iter = mXPCDocumentCache.Iter();
    MOZ_ASSERT(!iter.Done());
    xpcAccessibleDocument* xpcDoc = iter.UserData();
    NS_ASSERTION(xpcDoc, "No xpc doc for the object in xpc doc cache!");

    if (xpcDoc) {
      xpcDoc->Shutdown();
    }

    iter.Remove();
  }
}

void DocManager::RemoteDocAdded(DocAccessibleParent* aDoc) {
  if (!sRemoteDocuments) {
    sRemoteDocuments = new nsTArray<DocAccessibleParent*>;
    ClearOnShutdown(&sRemoteDocuments);
  }

  MOZ_ASSERT(!sRemoteDocuments->Contains(aDoc),
             "How did we already have the doc!");
  sRemoteDocuments->AppendElement(aDoc);
  ProxyCreated(aDoc);
}

DocAccessible* mozilla::a11y::GetExistingDocAccessible(
    const dom::Document* aDocument) {
  PresShell* presShell = aDocument->GetPresShell();
  return presShell ? presShell->GetDocAccessible() : nullptr;
}
