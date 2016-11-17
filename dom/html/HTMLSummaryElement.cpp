/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

NS_IMPL_NS_NEW_HTML_ELEMENT(Summary)

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

  if (!IsMainSummary()) {
    return rv;
  }

  WidgetEvent* const event = aVisitor.mEvent;

  if (event->HasMouseEventMessage()) {
    WidgetMouseEvent* mouseEvent = event->AsMouseEvent();

    if (mouseEvent->IsLeftClickEvent()) {
      RefPtr<HTMLDetailsElement> details = GetDetails();
      MOZ_ASSERT(details,
                 "Expected to find details since this is the main summary!");

      // When dispatching a synthesized mouse click event to a details element
      // with 'display: none', both Chrome and Safari do not toggle the 'open'
      // attribute. We had tried to be compatible with this behavior, but found
      // more inconsistency in test cases in bug 1245424. So we stop doing that.
      details->ToggleOpen();
      aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      return NS_OK;
    }
  } // event->HasMouseEventMessage()

  if (event->HasKeyEventMessage()) {
    WidgetKeyboardEvent* keyboardEvent = event->AsKeyboardEvent();
    bool dispatchClick = false;

    switch (event->mMessage) {
      case eKeyPress:
        if (keyboardEvent->mCharCode == ' ') {
          // Consume 'space' key to prevent scrolling the page down.
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }

        dispatchClick = keyboardEvent->mKeyCode == NS_VK_RETURN;
        break;

      case eKeyUp:
        dispatchClick = keyboardEvent->mKeyCode == NS_VK_SPACE;
        break;

      default:
        break;
    }

    if (dispatchClick) {
      rv = DispatchSimulatedClick(this, event->mFlags.mIsTrusted,
                                  aVisitor.mPresContext);
      if (NS_SUCCEEDED(rv)) {
        aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
  } // event->HasKeyEventMessage()

  return rv;
}

bool
HTMLSummaryElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                    int32_t* aTabIndex)
{
  bool disallowOverridingFocusability =
    nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex);

  if (disallowOverridingFocusability || !IsMainSummary()) {
    return disallowOverridingFocusability;
  }

#ifdef XP_MACOSX
  // The parent does not have strong opinion about the focusability of this main
  // summary element, but we'd like to override it when mouse clicking on Mac OS
  // like other form elements.
  *aIsFocusable = !aWithMouse || nsFocusManager::sMouseFocusesFormControl;
#else
  // The main summary element is focusable on other platforms.
  *aIsFocusable = true;
#endif

  // Give a chance to allow the subclass to override aIsFocusable.
  return false;
}

int32_t
HTMLSummaryElement::TabIndexDefault()
{
  // Make the main summary be able to navigate via tab, and be focusable.
  // See nsGenericHTMLElement::IsHTMLFocusable().
  return IsMainSummary() ? 0 : nsGenericHTMLElement::TabIndexDefault();
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
