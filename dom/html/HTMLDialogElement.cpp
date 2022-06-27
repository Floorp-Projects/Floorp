/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDialogElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLDialogElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/StaticPrefs_dom.h"

#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

// Expand NS_IMPL_NS_NEW_HTML_ELEMENT(Dialog) with pref check
nsGenericHTMLElement* NS_NewHTMLDialogElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  bool isChromeDocument = nsContentUtils::IsChromeDoc(nodeInfo->GetDocument());
  if (mozilla::StaticPrefs::dom_dialog_element_enabled() || isChromeDocument) {
    return new (nim) mozilla::dom::HTMLDialogElement(nodeInfo.forget());
  }
  return new (nim) mozilla::dom::HTMLUnknownElement(nodeInfo.forget());
}

namespace mozilla::dom {

HTMLDialogElement::~HTMLDialogElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLDialogElement)

bool HTMLDialogElement::IsDialogEnabled(JSContext* aCx,
                                        JS::Handle<JSObject*> aObj) {
  return StaticPrefs::dom_dialog_element_enabled() ||
         nsContentUtils::IsSystemCaller(aCx);
}

void HTMLDialogElement::Close(
    const mozilla::dom::Optional<nsAString>& aReturnValue) {
  if (!Open()) {
    return;
  }
  if (aReturnValue.WasPassed()) {
    SetReturnValue(aReturnValue.Value());
  }

  SetOpen(false, IgnoreErrors());

  RemoveFromTopLayerIfNeeded();

  RefPtr<Element> previouslyFocusedElement =
      do_QueryReferent(mPreviouslyFocusedElement);

  if (previouslyFocusedElement) {
    mPreviouslyFocusedElement = nullptr;

    FocusOptions options;
    options.mPreventScroll = true;
    previouslyFocusedElement->Focus(options, CallerType::NonSystem,
                                    IgnoredErrorResult());
  }

  RefPtr<AsyncEventDispatcher> eventDispatcher =
      new AsyncEventDispatcher(this, u"close"_ns, CanBubble::eNo);
  eventDispatcher->PostDOMEvent();
}

void HTMLDialogElement::Show() {
  if (Open()) {
    return;
  }
  SetOpen(true, IgnoreErrors());

  StorePreviouslyFocusedElement();

  FocusDialog();
}

bool HTMLDialogElement::IsInTopLayer() const {
  return State().HasState(ElementState::MODAL_DIALOG);
}

void HTMLDialogElement::AddToTopLayerIfNeeded() {
  MOZ_ASSERT(IsInComposedDoc());
  if (IsInTopLayer()) {
    return;
  }

  OwnerDoc()->AddModalDialog(*this);
}

void HTMLDialogElement::RemoveFromTopLayerIfNeeded() {
  if (!IsInTopLayer()) {
    return;
  }
  OwnerDoc()->RemoveModalDialog(*this);
}

void HTMLDialogElement::StorePreviouslyFocusedElement() {
  if (Document* doc = GetComposedDoc()) {
    if (nsIContent* unretargetedFocus = doc->GetUnretargetedFocusedContent()) {
      mPreviouslyFocusedElement =
          do_GetWeakReference(unretargetedFocus->AsElement());
    }
  }
}

void HTMLDialogElement::UnbindFromTree(bool aNullParent) {
  RemoveFromTopLayerIfNeeded();
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLDialogElement::ShowModal(ErrorResult& aError) {
  if (!IsInComposedDoc()) {
    return aError.ThrowInvalidStateError("Dialog element is not connected");
  }

  if (Open()) {
    return aError.ThrowInvalidStateError(
        "Dialog element already has an 'open' attribute");
  }

  AddToTopLayerIfNeeded();

  SetOpen(true, aError);

  StorePreviouslyFocusedElement();

  FocusDialog();

  aError.SuppressException();
}

void HTMLDialogElement::FocusDialog() {
  // 1) If subject is inert, return.
  // 2) Let control be the first descendant element of subject, in tree
  // order, that is not inert and has the autofocus attribute specified.
  if (RefPtr<Document> doc = GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  Element* controlCandidate = nullptr;
  for (auto* child = GetFirstChild(); child; child = child->GetNextNode(this)) {
    auto* element = Element::FromNode(child);
    if (!element) {
      continue;
    }
    nsIFrame* frame = element->GetPrimaryFrame();
    if (!frame || !frame->IsFocusable()) {
      continue;
    }
    if (element->HasAttr(nsGkAtoms::autofocus)) {
      // Find the first descendant of element of subject that this not inert and
      // has autofocus attribute.
      controlCandidate = element;
      break;
    }
    if (!controlCandidate) {
      // If there isn't one, then let control be the first non-inert descendant
      // element of subject, in tree order.
      controlCandidate = element;
    }
  }

  // If there isn't one of those either, then let control be subject.
  if (!controlCandidate) {
    controlCandidate = this;
  }

  RefPtr<Element> control = controlCandidate;

  // 3) Run the focusing steps for control.
  ErrorResult rv;
  nsIFrame* frame = control->GetPrimaryFrame();
  if (frame && frame->IsFocusable()) {
    control->Focus(FocusOptions(), CallerType::NonSystem, rv);
    if (rv.Failed()) {
      return;
    }
  } else if (IsInTopLayer()) {
    if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
      // Clear the focus which ends up making the body gets focused
      nsCOMPtr<nsPIDOMWindowOuter> outerWindow = OwnerDoc()->GetWindow();
      fm->ClearFocus(outerWindow);
    }
  }

  // 4) Let topDocument be the active document of control's node document's
  // browsing context's top-level browsing context.
  // 5) If control's node document's origin is not the same as the origin of
  // topDocument, then return.
  BrowsingContext* bc = control->OwnerDoc()->GetBrowsingContext();
  if (bc && bc->SameOriginWithTop()) {
    if (nsCOMPtr<nsIDocShell> docShell = bc->Top()->GetDocShell()) {
      if (Document* topDocument = docShell->GetExtantDocument()) {
        // 6) Empty topDocument's autofocus candidates.
        // 7) Set topDocument's autofocus processed flag to true.
        topDocument->SetAutoFocusFired();
      }
    }
  }
}

void HTMLDialogElement::QueueCancelDialog() {
  // queues an element task on the user interaction task source
  OwnerDoc()
      ->EventTargetFor(TaskCategory::UI)
      ->Dispatch(NewRunnableMethod("HTMLDialogElement::RunCancelDialogSteps",
                                   this,
                                   &HTMLDialogElement::RunCancelDialogSteps));
}

void HTMLDialogElement::RunCancelDialogSteps() {
  // 1) Let close be the result of firing an event named cancel at dialog, with
  // the cancelable attribute initialized to true.
  bool defaultAction = true;
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(), this, u"cancel"_ns,
                                       CanBubble::eNo, Cancelable::eYes,
                                       &defaultAction);

  // 2) If close is true and dialog has an open attribute, then close the dialog
  // with no return value.
  if (defaultAction) {
    Optional<nsAString> retValue;
    Close(retValue);
  }
}

JSObject* HTMLDialogElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLDialogElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
