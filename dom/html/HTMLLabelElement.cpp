/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of HTML <label> elements.
 */
#include "HTMLLabelElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/HTMLLabelElementBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "nsFocusManager.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "mozilla/dom/ShadowRoot.h"

// construction, destruction

NS_IMPL_NS_NEW_HTML_ELEMENT(Label)

namespace mozilla {
namespace dom {

HTMLLabelElement::~HTMLLabelElement()
{
}

JSObject*
HTMLLabelElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLLabelElementBinding::Wrap(aCx, this, aGivenProto);
}

// nsIDOMHTMLLabelElement

NS_IMPL_ELEMENT_CLONE(HTMLLabelElement)

HTMLFormElement*
HTMLLabelElement::GetForm() const
{
  nsGenericHTMLElement* control = GetControl();
  if (!control) {
    return nullptr;
  }

  // Not all labeled things have a form association.  Stick to the ones that do.
  nsCOMPtr<nsIFormControl> formControl = do_QueryObject(control);
  if (!formControl) {
    return nullptr;
  }

  return static_cast<HTMLFormElement*>(formControl->GetFormElement());
}

void
HTMLLabelElement::Focus(ErrorResult& aError)
{
  // retarget the focus method at the for content
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    RefPtr<Element> elem = GetLabeledElement();
    if (elem) {
      fm->SetFocus(elem, 0);
    }
  }
}

static bool
InInteractiveHTMLContent(nsIContent* aContent, nsIContent* aStop)
{
  nsIContent* content = aContent;
  while (content && content != aStop) {
    if (content->IsElement() &&
        content->AsElement()->IsInteractiveHTMLContent(true)) {
      return true;
    }
    content = content->GetParent();
  }
  return false;
}

nsresult
HTMLLabelElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  if (mHandlingEvent ||
      (!(mouseEvent && mouseEvent->IsLeftClickEvent()) &&
       aVisitor.mEvent->mMessage != eMouseDown) ||
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      !aVisitor.mPresContext ||
      // Don't handle the event if it's already been handled by another label
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> target = do_QueryInterface(aVisitor.mEvent->mTarget);
  if (InInteractiveHTMLContent(target, this)) {
    return NS_OK;
  }

  // Strong ref because event dispatch is going to happen.
  RefPtr<Element> content = GetLabeledElement();

  if (content) {
    mHandlingEvent = true;
    switch (aVisitor.mEvent->mMessage) {
      case eMouseDown:
        if (mouseEvent->button == WidgetMouseEvent::eLeftButton) {
          // We reset the mouse-down point on every event because there is
          // no guarantee we will reach the eMouseClick code below.
          LayoutDeviceIntPoint* curPoint =
            new LayoutDeviceIntPoint(mouseEvent->mRefPoint);
          SetProperty(nsGkAtoms::labelMouseDownPtProperty,
                      static_cast<void*>(curPoint),
                      nsINode::DeleteProperty<LayoutDeviceIntPoint>);
        }
        break;

      case eMouseClick:
        if (mouseEvent->IsLeftClickEvent()) {
          LayoutDeviceIntPoint* mouseDownPoint =
            static_cast<LayoutDeviceIntPoint*>(
              GetProperty(nsGkAtoms::labelMouseDownPtProperty));

          bool dragSelect = false;
          if (mouseDownPoint) {
            LayoutDeviceIntPoint dragDistance = *mouseDownPoint;
            DeleteProperty(nsGkAtoms::labelMouseDownPtProperty);

            dragDistance -= mouseEvent->mRefPoint;
            const int CLICK_DISTANCE = 2;
            dragSelect = dragDistance.x > CLICK_DISTANCE ||
                         dragDistance.x < -CLICK_DISTANCE ||
                         dragDistance.y > CLICK_DISTANCE ||
                         dragDistance.y < -CLICK_DISTANCE;
          }
          // Don't click the for-content if we did drag-select text or if we
          // have a kbd modifier (which adjusts a selection).
          if (dragSelect || mouseEvent->IsShift() || mouseEvent->IsControl() ||
              mouseEvent->IsAlt() || mouseEvent->IsMeta()) {
            break;
          }
          // Only set focus on the first click of multiple clicks to prevent
          // to prevent immediate de-focus.
          if (mouseEvent->mClickCount <= 1) {
            nsIFocusManager* fm = nsFocusManager::GetFocusManager();
            if (fm) {
              // Use FLAG_BYMOVEFOCUS here so that the label is scrolled to.
              // Also, within HTMLInputElement::PostHandleEvent, inputs will
              // be selected only when focused via a key or when the navigation
              // flag is used and we want to select the text on label clicks as
              // well.
              // If the label has been clicked by the user, we also want to
              // pass FLAG_BYMOUSE so that we get correct focus ring behavior,
              // but we don't want to pass FLAG_BYMOUSE if this click event was
              // caused by the user pressing an accesskey.
              bool byMouse = (mouseEvent->inputSource != MouseEventBinding::MOZ_SOURCE_KEYBOARD);
              bool byTouch = (mouseEvent->inputSource == MouseEventBinding::MOZ_SOURCE_TOUCH);
              fm->SetFocus(content,
                           nsIFocusManager::FLAG_BYMOVEFOCUS |
                           (byMouse ? nsIFocusManager::FLAG_BYMOUSE : 0) |
                           (byTouch ? nsIFocusManager::FLAG_BYTOUCH : 0));
            }
          }
          // Dispatch a new click event to |content|
          //    (For compatibility with IE, we do only left click.  If
          //    we wanted to interpret the HTML spec very narrowly, we
          //    would do nothing.  If we wanted to do something
          //    sensible, we might send more events through like
          //    this.)  See bug 7554, bug 49897, and bug 96813.
          nsEventStatus status = aVisitor.mEventStatus;
          // Ok to use aVisitor.mEvent as parameter because DispatchClickEvent
          // will actually create a new event.
          EventFlags eventFlags;
          eventFlags.mMultipleActionsPrevented = true;
          DispatchClickEvent(aVisitor.mPresContext, mouseEvent,
                             content, false, &eventFlags, &status);
          // Do we care about the status this returned?  I don't think we do...
          // Don't run another <label> off of this click
          mouseEvent->mFlags.mMultipleActionsPrevented = true;
        }
        break;

      default:
        break;
    }
    mHandlingEvent = false;
  }
  return NS_OK;
}

bool
HTMLLabelElement::PerformAccesskey(bool aKeyCausesActivation,
                                   bool aIsTrustedEvent)
{
  if (!aKeyCausesActivation) {
    RefPtr<Element> element = GetLabeledElement();
    if (element) {
      return element->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
    }
  } else {
    nsPresContext *presContext = GetPresContext(eForUncomposedDoc);
    if (!presContext) {
      return false;
    }

    // Click on it if the users prefs indicate to do so.
    WidgetMouseEvent event(aIsTrustedEvent, eMouseClick,
                           nullptr, WidgetMouseEvent::eReal);
    event.inputSource = MouseEventBinding::MOZ_SOURCE_KEYBOARD;

    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);

    EventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                              &event);
  }

  return aKeyCausesActivation;
}

nsGenericHTMLElement*
HTMLLabelElement::GetLabeledElement() const
{
  nsAutoString elementId;

  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::_for, elementId)) {
    // No @for, so we are a label for our first form control element.
    // Do a depth-first traversal to look for the first form control element.
    return GetFirstLabelableDescendant();
  }

  // We have a @for. The id has to be linked to an element in the same tree
  // and this element should be a labelable form control.
  Element* element = nullptr;

  if (ShadowRoot* shadowRoot = GetContainingShadow()) {
    element = shadowRoot->GetElementById(elementId);
  } else if (nsIDocument* doc = GetUncomposedDoc()) {
    element = doc->GetElementById(elementId);
  } else {
    element = nsContentUtils::MatchElementId(SubtreeRoot()->AsContent(), elementId);
  }

  if (element && element->IsLabelable()) {
    return static_cast<nsGenericHTMLElement*>(element);
  }

  return nullptr;
}

nsGenericHTMLElement*
HTMLLabelElement::GetFirstLabelableDescendant() const
{
  for (nsIContent* cur = nsINode::GetFirstChild(); cur;
       cur = cur->GetNextNode(this)) {
    Element* element = cur->IsElement() ? cur->AsElement() : nullptr;
    if (element && element->IsLabelable()) {
      return static_cast<nsGenericHTMLElement*>(element);
    }
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla
