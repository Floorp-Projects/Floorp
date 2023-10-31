/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDialogElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLDialogElementBinding.h"

#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Dialog)

namespace mozilla::dom {

HTMLDialogElement::~HTMLDialogElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLDialogElement)

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

void HTMLDialogElement::Show(ErrorResult& aError) {
  if (Open()) {
    if (!IsInTopLayer()) {
      return;
    }
    return aError.ThrowInvalidStateError(
        "Cannot call show() on an open modal dialog.");
  }

  SetOpen(true, IgnoreErrors());

  StorePreviouslyFocusedElement();

  OwnerDoc()->HideAllPopoversWithoutRunningScript();
  FocusDialog();
}

bool HTMLDialogElement::IsInTopLayer() const {
  return State().HasState(ElementState::MODAL);
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
  if (Element* element = nsFocusManager::GetFocusedElementStatic()) {
    if (NS_SUCCEEDED(nsContentUtils::CheckSameOrigin(this, element))) {
      mPreviouslyFocusedElement = do_GetWeakReference(element);
    }
  } else if (Document* doc = GetComposedDoc()) {
    // Looks like there's a discrepancy sometimes when focus is moved
    // to a different in-process window.
    if (nsIContent* unretargetedFocus = doc->GetUnretargetedFocusedContent()) {
      mPreviouslyFocusedElement = do_GetWeakReference(unretargetedFocus);
    }
  }
}

void HTMLDialogElement::UnbindFromTree(bool aNullParent) {
  RemoveFromTopLayerIfNeeded();
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLDialogElement::ShowModal(ErrorResult& aError) {
  if (Open()) {
    if (IsInTopLayer()) {
      return;
    }
    return aError.ThrowInvalidStateError(
        "Cannot call showModal() on an open non-modal dialog.");
  }

  if (!IsInComposedDoc()) {
    return aError.ThrowInvalidStateError("Dialog element is not connected");
  }

  if (IsPopoverOpen()) {
    return aError.ThrowInvalidStateError(
        "Dialog element is already an open popover.");
  }

  AddToTopLayerIfNeeded();

  SetOpen(true, aError);

  StorePreviouslyFocusedElement();

  OwnerDoc()->HideAllPopoversWithoutRunningScript();
  FocusDialog();

  aError.SuppressException();
}

void HTMLDialogElement::FocusDialog() {
  // 1) If subject is inert, return.
  // 2) Let control be the first descendant element of subject, in tree
  // order, that is not inert and has the autofocus attribute specified.
  RefPtr<Document> doc = OwnerDoc();
  if (IsInComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  RefPtr<Element> control = HasAttr(nsGkAtoms::autofocus)
                                ? this
                                : GetFocusDelegate(false /* aWithMouse */);

  // If there isn't one of those either, then let control be subject.
  if (!control) {
    control = this;
  }

  FocusCandidate(control, IsInTopLayer());
}

int32_t HTMLDialogElement::TabIndexDefault() { return 0; }

void HTMLDialogElement::QueueCancelDialog() {
  // queues an element task on the user interaction task source
  OwnerDoc()->Dispatch(
      NewRunnableMethod("HTMLDialogElement::RunCancelDialogSteps", this,
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
