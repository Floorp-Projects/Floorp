/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSummaryElement.h"

#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "nsFocusManager.h"
#include "nsGenericHTMLElement.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Summary)

namespace mozilla::dom {

HTMLSummaryElement::~HTMLSummaryElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLSummaryElement)

void HTMLSummaryElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  if (WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent()) {
    aVisitor.mWantsActivationBehavior = mouseEvent->IsLeftClickEvent();
  }

  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

bool HTMLSummaryElement::CheckHandleEventPreconditions(
    EventChainVisitor& aVisitor) {
  if (!aVisitor.mPresContext ||
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      !IsMainSummary()) {
    return false;
  }

  nsCOMPtr<Element> target =
      do_QueryInterface(aVisitor.mEvent->GetOriginalDOMEventTarget());
  return !nsContentUtils::IsInInteractiveHTMLContent(target, this);
}

nsresult HTMLSummaryElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  nsresult rv = NS_OK;
  if (!CheckHandleEventPreconditions(aVisitor)) {
    return rv;
  }

  if (aVisitor.mEvent->HasKeyEventMessage() && aVisitor.mEvent->IsTrusted()) {
    HandleKeyboardActivation(aVisitor);
  }
  return rv;
}

void HTMLSummaryElement::ActivationBehavior(EventChainPostVisitor& aVisitor) {
  if (!CheckHandleEventPreconditions(aVisitor)) {
    return;
  }

  DebugOnly<WidgetMouseEvent*> mouseEvent = aVisitor.mEvent->AsMouseEvent();
  MOZ_ASSERT(mouseEvent && mouseEvent->IsLeftClickEvent(),
             "ActivationBehavior should only be called for left click.");

  RefPtr<HTMLDetailsElement> details = GetDetails();
  MOZ_ASSERT(details,
             "Expected to find details since this is the main summary!");

  // When dispatching a synthesized mouse click event to a details element
  // with 'display: none', both Chrome and Safari do not toggle the 'open'
  // attribute. We had tried to be compatible with this behavior, but found
  // more inconsistency in test cases in bug 1245424. So we stop doing that.
  details->ToggleOpen();
  aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
}

bool HTMLSummaryElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                         int32_t* aTabIndex) {
  bool disallowOverridingFocusability = nsGenericHTMLElement::IsHTMLFocusable(
      aWithMouse, aIsFocusable, aTabIndex);

  if (disallowOverridingFocusability || !IsMainSummary()) {
    return disallowOverridingFocusability;
  }

  // The main summary element is focusable.
  *aIsFocusable = true;

  // Give a chance to allow the subclass to override aIsFocusable.
  return false;
}

int32_t HTMLSummaryElement::TabIndexDefault() {
  // Make the main summary be able to navigate via tab, and be focusable.
  // See nsGenericHTMLElement::IsHTMLFocusable().
  return IsMainSummary() ? 0 : nsGenericHTMLElement::TabIndexDefault();
}

bool HTMLSummaryElement::IsMainSummary() const {
  HTMLDetailsElement* details = GetDetails();
  if (!details) {
    return false;
  }

  return details->GetFirstSummary() == this ||
         GetContainingShadow() == details->GetShadowRoot();
}

HTMLDetailsElement* HTMLSummaryElement::GetDetails() const {
  if (auto* details = HTMLDetailsElement::FromNodeOrNull(GetParent())) {
    return details;
  }
  if (!HasBeenInUAWidget()) {
    return nullptr;
  }
  return HTMLDetailsElement::FromNodeOrNull(GetContainingShadowHost());
}

JSObject* HTMLSummaryElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return HTMLElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
