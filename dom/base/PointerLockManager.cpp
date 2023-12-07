/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PointerLockManager.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_full_screen_api.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/WindowContext.h"
#include "nsCOMPtr.h"
#include "nsSandboxFlags.h"

namespace mozilla {

using mozilla::dom::BrowserChild;
using mozilla::dom::BrowserParent;
using mozilla::dom::BrowsingContext;
using mozilla::dom::CallerType;
using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::WindowContext;

// Reference to the pointer locked element.
static nsWeakPtr sLockedElement;

// Reference to the document which requested pointer lock.
static nsWeakPtr sLockedDoc;

// Reference to the BrowserParent requested pointer lock.
static BrowserParent* sLockedRemoteTarget = nullptr;

/* static */
bool PointerLockManager::sIsLocked = false;

/* static */
already_AddRefed<dom::Element> PointerLockManager::GetLockedElement() {
  nsCOMPtr<Element> element = do_QueryReferent(sLockedElement);
  return element.forget();
}

/* static */
already_AddRefed<dom::Document> PointerLockManager::GetLockedDocument() {
  nsCOMPtr<Document> document = do_QueryReferent(sLockedDoc);
  return document.forget();
}

/* static */
BrowserParent* PointerLockManager::GetLockedRemoteTarget() {
  MOZ_ASSERT(XRE_IsParentProcess());
  return sLockedRemoteTarget;
}

static void DispatchPointerLockChange(Document* aTarget) {
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(aTarget, u"pointerlockchange"_ns,
                               CanBubble::eYes, ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
}

static void DispatchPointerLockError(Document* aTarget, const char* aMessage) {
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(aTarget, u"pointerlockerror"_ns, CanBubble::eYes,
                               ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                  aTarget, nsContentUtils::eDOM_PROPERTIES,
                                  aMessage);
}

static const char* GetPointerLockError(Element* aElement, Element* aCurrentLock,
                                       bool aNoFocusCheck = false) {
  // Check if pointer lock pref is enabled
  if (!StaticPrefs::full_screen_api_pointer_lock_enabled()) {
    return "PointerLockDeniedDisabled";
  }

  nsCOMPtr<Document> ownerDoc = aElement->OwnerDoc();
  if (aCurrentLock && aCurrentLock->OwnerDoc() != ownerDoc) {
    return "PointerLockDeniedInUse";
  }

  if (!aElement->IsInComposedDoc()) {
    return "PointerLockDeniedNotInDocument";
  }

  if (ownerDoc->GetSandboxFlags() & SANDBOXED_POINTER_LOCK) {
    return "PointerLockDeniedSandboxed";
  }

  // Check if the element is in a document with a docshell.
  if (!ownerDoc->GetContainer()) {
    return "PointerLockDeniedHidden";
  }
  nsCOMPtr<nsPIDOMWindowOuter> ownerWindow = ownerDoc->GetWindow();
  if (!ownerWindow) {
    return "PointerLockDeniedHidden";
  }
  nsCOMPtr<nsPIDOMWindowInner> ownerInnerWindow = ownerDoc->GetInnerWindow();
  if (!ownerInnerWindow) {
    return "PointerLockDeniedHidden";
  }
  if (ownerWindow->GetCurrentInnerWindow() != ownerInnerWindow) {
    return "PointerLockDeniedHidden";
  }

  BrowsingContext* bc = ownerDoc->GetBrowsingContext();
  BrowsingContext* topBC = bc ? bc->Top() : nullptr;
  WindowContext* topWC = ownerDoc->GetTopLevelWindowContext();
  if (!topBC || !topBC->IsActive() || !topWC ||
      topWC != topBC->GetCurrentWindowContext()) {
    return "PointerLockDeniedHidden";
  }

  if (!aNoFocusCheck) {
    if (!IsInActiveTab(ownerDoc)) {
      return "PointerLockDeniedNotFocused";
    }
  }

  return nullptr;
}

/* static */
void PointerLockManager::RequestLock(Element* aElement,
                                     CallerType aCallerType) {
  NS_ASSERTION(aElement,
               "Must pass non-null element to PointerLockManager::RequestLock");

  RefPtr<Document> doc = aElement->OwnerDoc();
  nsCOMPtr<Element> pointerLockedElement = GetLockedElement();
  if (aElement == pointerLockedElement) {
    DispatchPointerLockChange(doc);
    return;
  }

  if (const char* msg = GetPointerLockError(aElement, pointerLockedElement)) {
    DispatchPointerLockError(doc, msg);
    return;
  }

  bool userInputOrSystemCaller =
      doc->HasValidTransientUserGestureActivation() ||
      aCallerType == CallerType::System;
  nsCOMPtr<nsIRunnable> request =
      new PointerLockRequest(aElement, userInputOrSystemCaller);
  doc->Dispatch(request.forget());
}

/* static */
void PointerLockManager::Unlock(Document* aDoc) {
  if (!sIsLocked) {
    return;
  }

  nsCOMPtr<Document> pointerLockedDoc = GetLockedDocument();
  if (!pointerLockedDoc || (aDoc && aDoc != pointerLockedDoc)) {
    return;
  }
  if (!SetPointerLock(nullptr, pointerLockedDoc, StyleCursorKind::Auto)) {
    return;
  }

  nsCOMPtr<Element> pointerLockedElement = GetLockedElement();
  ChangePointerLockedElement(nullptr, pointerLockedDoc, pointerLockedElement);

  if (BrowserChild* browserChild =
          BrowserChild::GetFrom(pointerLockedDoc->GetDocShell())) {
    browserChild->SendReleasePointerLock();
  }

  AsyncEventDispatcher::RunDOMEventWhenSafe(
      *pointerLockedElement, u"MozDOMPointerLock:Exited"_ns, CanBubble::eYes,
      ChromeOnlyDispatch::eYes);
}

/* static */
void PointerLockManager::ChangePointerLockedElement(
    Element* aElement, Document* aDocument, Element* aPointerLockedElement) {
  // aDocument here is not really necessary, as it is the uncomposed
  // document of both aElement and aPointerLockedElement as far as one
  // is not nullptr, and they wouldn't both be nullptr in any case.
  // But since the caller of this function should have known what the
  // document is, we just don't try to figure out what it should be.
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(aElement != aPointerLockedElement);
  if (aPointerLockedElement) {
    MOZ_ASSERT(aPointerLockedElement->GetComposedDoc() == aDocument);
    aPointerLockedElement->ClearPointerLock();
  }
  if (aElement) {
    MOZ_ASSERT(aElement->GetComposedDoc() == aDocument);
    aElement->SetPointerLock();
    sLockedElement = do_GetWeakReference(aElement);
    sLockedDoc = do_GetWeakReference(aDocument);
    NS_ASSERTION(sLockedElement && sLockedDoc,
                 "aElement and this should support weak references!");
  } else {
    sLockedElement = nullptr;
    sLockedDoc = nullptr;
  }
  // Retarget all events to aElement via capture or
  // stop retargeting if aElement is nullptr.
  PresShell::SetCapturingContent(aElement, CaptureFlags::PointerLock);
  DispatchPointerLockChange(aDocument);
}

/* static */
bool PointerLockManager::StartSetPointerLock(Element* aElement,
                                             Document* aDocument) {
  if (!SetPointerLock(aElement, aDocument, StyleCursorKind::None)) {
    DispatchPointerLockError(aDocument, "PointerLockDeniedFailedToLock");
    return false;
  }

  ChangePointerLockedElement(aElement, aDocument, nullptr);
  nsContentUtils::DispatchEventOnlyToChrome(
      aDocument, aElement, u"MozDOMPointerLock:Entered"_ns, CanBubble::eYes,
      Cancelable::eNo, /* DefaultAction */ nullptr);

  return true;
}

/* static */
bool PointerLockManager::SetPointerLock(Element* aElement, Document* aDocument,
                                        StyleCursorKind aCursorStyle) {
  MOZ_ASSERT(!aElement || aElement->OwnerDoc() == aDocument,
             "We should be either unlocking pointer (aElement is nullptr), "
             "or locking pointer to an element in this document");
#ifdef DEBUG
  if (!aElement) {
    nsCOMPtr<Document> pointerLockedDoc = GetLockedDocument();
    MOZ_ASSERT(pointerLockedDoc == aDocument);
  }
#endif

  PresShell* presShell = aDocument->GetPresShell();
  if (!presShell) {
    NS_WARNING("SetPointerLock(): No PresShell");
    if (!aElement) {
      sIsLocked = false;
      // If we are unlocking pointer lock, but for some reason the doc
      // has already detached from the presshell, just ask the event
      // state manager to release the pointer.
      EventStateManager::SetPointerLock(nullptr, nullptr);
      return true;
    }
    return false;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    NS_WARNING("SetPointerLock(): Unable to get PresContext");
    return false;
  }

  nsCOMPtr<nsIWidget> widget;
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!NS_WARN_IF(!rootFrame)) {
    widget = rootFrame->GetNearestWidget();
    NS_WARNING_ASSERTION(widget,
                         "SetPointerLock(): Unable to find widget in "
                         "presShell->GetRootFrame()->GetNearestWidget();");
    if (aElement && !widget) {
      return false;
    }
  }

  sIsLocked = !!aElement;

  // Hide the cursor and set pointer lock for future mouse events
  RefPtr<EventStateManager> esm = presContext->EventStateManager();
  esm->SetCursor(aCursorStyle, nullptr, {}, Nothing(), widget, true);
  EventStateManager::SetPointerLock(widget, aElement);

  return true;
}

/* static */
bool PointerLockManager::IsInLockContext(BrowsingContext* aContext) {
  if (!aContext) {
    return false;
  }

  nsCOMPtr<Document> pointerLockedDoc = GetLockedDocument();
  if (!pointerLockedDoc || !pointerLockedDoc->GetBrowsingContext()) {
    return false;
  }

  BrowsingContext* lockTop = pointerLockedDoc->GetBrowsingContext()->Top();
  BrowsingContext* top = aContext->Top();

  return top == lockTop;
}

/* static */
bool PointerLockManager::SetLockedRemoteTarget(BrowserParent* aBrowserParent) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (sLockedRemoteTarget) {
    return sLockedRemoteTarget == aBrowserParent;
  }

  sLockedRemoteTarget = aBrowserParent;
  return true;
}

/* static */
void PointerLockManager::ReleaseLockedRemoteTarget(
    BrowserParent* aBrowserParent) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (sLockedRemoteTarget == aBrowserParent) {
    sLockedRemoteTarget = nullptr;
  }
}

PointerLockManager::PointerLockRequest::PointerLockRequest(
    Element* aElement, bool aUserInputOrChromeCaller)
    : mozilla::Runnable("PointerLockRequest"),
      mElement(do_GetWeakReference(aElement)),
      mDocument(do_GetWeakReference(aElement->OwnerDoc())),
      mUserInputOrChromeCaller(aUserInputOrChromeCaller) {}

NS_IMETHODIMP
PointerLockManager::PointerLockRequest::Run() {
  nsCOMPtr<Element> element = do_QueryReferent(mElement);
  nsCOMPtr<Document> document = do_QueryReferent(mDocument);

  const char* error = nullptr;
  if (!element || !document || !element->GetComposedDoc()) {
    error = "PointerLockDeniedNotInDocument";
  } else if (element->GetComposedDoc() != document) {
    error = "PointerLockDeniedMovedDocument";
  }
  if (!error) {
    nsCOMPtr<Element> pointerLockedElement = do_QueryReferent(sLockedElement);
    if (element == pointerLockedElement) {
      DispatchPointerLockChange(document);
      return NS_OK;
    }
    // Note, we must bypass focus change, so pass true as the last parameter!
    error = GetPointerLockError(element, pointerLockedElement, true);
    // Another element in the same document is requesting pointer lock,
    // just grant it without user input check.
    if (!error && pointerLockedElement) {
      ChangePointerLockedElement(element, document, pointerLockedElement);
      return NS_OK;
    }
  }
  // If it is neither user input initiated, nor requested in fullscreen,
  // it should be rejected.
  if (!error && !mUserInputOrChromeCaller && !document->Fullscreen()) {
    error = "PointerLockDeniedNotInputDriven";
  }

  if (error) {
    DispatchPointerLockError(document, error);
    return NS_OK;
  }

  if (BrowserChild* browserChild =
          BrowserChild::GetFrom(document->GetDocShell())) {
    nsWeakPtr e = do_GetWeakReference(element);
    nsWeakPtr doc = do_GetWeakReference(element->OwnerDoc());
    nsWeakPtr bc = do_GetWeakReference(browserChild);
    browserChild->SendRequestPointerLock(
        [e, doc, bc](const nsCString& aError) {
          nsCOMPtr<Document> document = do_QueryReferent(doc);
          if (!aError.IsEmpty()) {
            DispatchPointerLockError(document, aError.get());
            return;
          }

          const char* error = nullptr;
          auto autoCleanup = MakeScopeExit([&] {
            if (error) {
              DispatchPointerLockError(document, error);
              // If we are failed to set pointer lock, notify parent to stop
              // redirect mouse event to this process.
              if (nsCOMPtr<nsIBrowserChild> browserChild =
                      do_QueryReferent(bc)) {
                static_cast<BrowserChild*>(browserChild.get())
                    ->SendReleasePointerLock();
              }
            }
          });

          nsCOMPtr<Element> element = do_QueryReferent(e);
          if (!element || !document || !element->GetComposedDoc()) {
            error = "PointerLockDeniedNotInDocument";
            return;
          }

          if (element->GetComposedDoc() != document) {
            error = "PointerLockDeniedMovedDocument";
            return;
          }

          nsCOMPtr<Element> pointerLockedElement = GetLockedElement();
          error = GetPointerLockError(element, pointerLockedElement, true);
          if (error) {
            return;
          }

          if (!StartSetPointerLock(element, document)) {
            error = "PointerLockDeniedFailedToLock";
            return;
          }
        },
        [doc](mozilla::ipc::ResponseRejectReason) {
          // IPC layer error
          nsCOMPtr<Document> document = do_QueryReferent(doc);
          if (!document) {
            return;
          }

          DispatchPointerLockError(document, "PointerLockDeniedFailedToLock");
        });
  } else {
    StartSetPointerLock(element, document);
  }

  return NS_OK;
}

}  // namespace mozilla
