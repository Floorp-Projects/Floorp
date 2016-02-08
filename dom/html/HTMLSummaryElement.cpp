/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSummaryElement.h"

#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"

// Expand NS_IMPL_NS_NEW_HTML_ELEMENT(Summary) to add pref check.
nsGenericHTMLElement*
NS_NewHTMLSummaryElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         mozilla::dom::FromParser aFromParser)
{
  if (!mozilla::dom::HTMLDetailsElement::IsDetailsEnabled()) {
    return new mozilla::dom::HTMLUnknownElement(aNodeInfo);
  }

  return new mozilla::dom::HTMLSummaryElement(aNodeInfo);
}

namespace mozilla {
namespace dom {

HTMLSummaryElement::~HTMLSummaryElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLSummaryElement)

nsresult
HTMLSummaryElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  nsresult rv = NS_OK;
  if (!aVisitor.mPresContext) {
    return rv;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return rv;
  }

  auto toggleDetails = false;
  auto* event = aVisitor.mEvent;

  if (event->HasMouseEventMessage()) {
    auto* mouseEvent = event->AsMouseEvent();
    toggleDetails = mouseEvent->IsLeftClickEvent();
  }

  // Todo: Bug 634004: Implement toggle details by keyboard.

  if (!toggleDetails || !IsMainSummary()) {
    return rv;
  }

  auto* details = GetDetails();
  MOZ_ASSERT(details, "Expected to find details since this is the main summary!");

  // When dispatching a synthesized mouse click event to a details with
  // 'display: none', both Chrome and Safari do not toggle the 'open' attribute.
  // We follow them by checking whether details has a frame or not.
  if (details->GetPrimaryFrame()) {
    details->ToggleOpen();
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
  }

  return rv;
}

bool
HTMLSummaryElement::IsMainSummary() const
{
  HTMLDetailsElement* details = GetDetails();
  if (!details) {
    return false;
  }

  return details->GetFirstSummary() == this || IsRootOfNativeAnonymousSubtree();
}

HTMLDetailsElement*
HTMLSummaryElement::GetDetails() const
{
  return HTMLDetailsElement::FromContentOrNull(GetParent());
}

JSObject*
HTMLSummaryElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
