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

/**
 * Implementation of HTML <label> elements.
 */
#include "nsHTMLLabelElement.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"

// construction, destruction

using namespace mozilla::dom;

NS_IMPL_NS_NEW_HTML_ELEMENT(Label)


nsHTMLLabelElement::nsHTMLLabelElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mHandlingEvent(PR_FALSE)
{
}

nsHTMLLabelElement::~nsHTMLLabelElement()
{
}

// nsISupports 


NS_IMPL_ADDREF_INHERITED(nsHTMLLabelElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLabelElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLLabelElement, nsHTMLLabelElement)

// QueryInterface implementation for nsHTMLLabelElement
NS_INTERFACE_TABLE_HEAD(nsHTMLLabelElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLLabelElement,
                                   nsIDOMHTMLLabelElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLLabelElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLLabelElement)


// nsIDOMHTMLLabelElement


NS_IMPL_ELEMENT_CLONE(nsHTMLLabelElement)


NS_IMETHODIMP
nsHTMLLabelElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMETHODIMP
nsHTMLLabelElement::GetControl(nsIDOMHTMLElement** aElement)
{
  *aElement = nsnull;

  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(GetLabeledElement());

  element.swap(*aElement);
  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLLabelElement, HtmlFor, _for)

NS_IMETHODIMP
nsHTMLLabelElement::Focus()
{
  // retarget the focus method at the for content
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(GetLabeledElement());
    if (elem)
      fm->SetFocus(elem, 0);
  }

  return NS_OK;
}

nsresult
nsHTMLLabelElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               PRBool aCompileEventHandlers)
{
  return nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                              aBindingParent,
                                              aCompileEventHandlers);
}

void
nsHTMLLabelElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}

static PRBool
EventTargetIn(nsEvent *aEvent, nsIContent *aChild, nsIContent *aStop)
{
  nsCOMPtr<nsIContent> c = do_QueryInterface(aEvent->target);
  nsIContent *content = c;
  while (content) {
    if (content == aChild) {
      return PR_TRUE;
    }

    if (content == aStop) {
      break;
    }

    content = content->GetParent();
  }
  return PR_FALSE;
}

static void
DestroyMouseDownPoint(void *    /*aObject*/,
                      nsIAtom * /*aPropertyName*/,
                      void *    aPropertyValue,
                      void *    /*aData*/)
{
  nsIntPoint *pt = (nsIntPoint *)aPropertyValue;
  delete pt;
}

nsresult
nsHTMLLabelElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (mHandlingEvent ||
      (!NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) &&
       aVisitor.mEvent->message != NS_MOUSE_BUTTON_DOWN) ||
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      !aVisitor.mPresContext ||
      // Don't handle the event if it's already been handled by another label
      (aVisitor.mEvent->flags & NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS)) {
    return NS_OK;
  }

  // Strong ref because event dispatch is going to happen.
  nsRefPtr<Element> content = GetLabeledElement();

  if (content && !EventTargetIn(aVisitor.mEvent, content, this)) {
    mHandlingEvent = PR_TRUE;
    switch (aVisitor.mEvent->message) {
      case NS_MOUSE_BUTTON_DOWN:
        NS_ASSERTION(aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT,
                     "wrong event struct for event");
        if (static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
            nsMouseEvent::eLeftButton) {
          // We reset the mouse-down point on every event because there is
          // no guarantee we will reach the NS_MOUSE_CLICK code below.
          nsIntPoint *curPoint = new nsIntPoint(aVisitor.mEvent->refPoint);
          SetProperty(nsGkAtoms::labelMouseDownPtProperty,
                      static_cast<void *>(curPoint),
                      DestroyMouseDownPoint);
        }
        break;

      case NS_MOUSE_CLICK:
        if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
          const nsMouseEvent* event =
            static_cast<const nsMouseEvent*>(aVisitor.mEvent);
          nsIntPoint *mouseDownPoint = static_cast<nsIntPoint *>
            (GetProperty(nsGkAtoms::labelMouseDownPtProperty));

          PRBool dragSelect = PR_FALSE;
          if (mouseDownPoint) {
            nsIntPoint dragDistance = *mouseDownPoint;
            DeleteProperty(nsGkAtoms::labelMouseDownPtProperty);

            dragDistance -= aVisitor.mEvent->refPoint;
            const int CLICK_DISTANCE = 2;
            dragSelect = dragDistance.x > CLICK_DISTANCE ||
                         dragDistance.x < -CLICK_DISTANCE ||
                         dragDistance.y > CLICK_DISTANCE ||
                         dragDistance.y < -CLICK_DISTANCE;
          }

          // Don't click the for-content if we did drag-select text or if we
          // have a kbd modifier (which adjusts a selection), or if it's a
          // double click (we already forwarded the first click event).
          if (dragSelect || event->clickCount > 1 ||
              event->isShift || event->isControl || event->isAlt ||
              event->isMeta) {
            break;
          }

          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            // Use FLAG_BYMOVEFOCUS here so that the label is scrolled to.
            // Also, within nsHTMLInputElement::PostHandleEvent, inputs will
            // be selected only when focused via a key or when the navigation
            // flag is used and we want to select the text on label clicks as
            // well.
            nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(content);
            fm->SetFocus(elem, nsIFocusManager::FLAG_BYMOVEFOCUS);
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
          DispatchClickEvent(aVisitor.mPresContext,
                             static_cast<nsInputEvent*>(aVisitor.mEvent),
                             content, PR_FALSE,
                             NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS, &status);
          // Do we care about the status this returned?  I don't think we do...
          // Don't run another <label> off of this click
          aVisitor.mEvent->flags |= NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS;
        }
        break;
    }
    mHandlingEvent = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsHTMLLabelElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLabelElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  return NS_OK;
}

nsresult
nsHTMLLabelElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                            const nsAString& aValue, PRBool aNotify)
{
  return nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                           aNotify);
}

nsresult
nsHTMLLabelElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRBool aNotify)
{
  return nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

void
nsHTMLLabelElement::PerformAccesskey(PRBool aKeyCausesActivation,
                                     PRBool aIsTrustedEvent)
{
  if (!aKeyCausesActivation) {
    nsRefPtr<Element> element = GetLabeledElement();
    if (element)
      element->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
  } else {
    nsPresContext *presContext = GetPresContext();
    if (!presContext)
      return;

    // Click on it if the users prefs indicate to do so.
    nsMouseEvent event(aIsTrustedEvent, NS_MOUSE_CLICK,
                       nsnull, nsMouseEvent::eReal);
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;

    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);

    nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                                &event);
  }
}

Element*
nsHTMLLabelElement::GetLabeledElement()
{
  nsAutoString elementId;

  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::_for, elementId)) {
    // No @for, so we are a label for our first form control element.
    // Do a depth-first traversal to look for the first form control element.
    return GetFirstDescendantFormControl();
  }

  // We have a @for. The id has to be linked to an element in the same document
  // and this element should be a labelable form control.
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }

  Element* element = doc->GetElementById(elementId);
  if (!element) {
    return nsnull;
  }

  nsCOMPtr<nsIFormControl> controlElement = do_QueryInterface(element);
  if (controlElement && controlElement->IsLabelableControl()) {
    // Transfer the reference count of element to the returned value.
    return element;
  }

  return nsnull;
}

Element*
nsHTMLLabelElement::GetFirstDescendantFormControl()
{
  // Have to cast do disambiguate GetFirstChild from the DOM method of that name
  for (nsINode* cur = static_cast<nsINode*>(this)->GetFirstChild();
       cur;
       cur = cur->GetNextNode(this)) {
    nsCOMPtr<nsIFormControl> element = do_QueryInterface(cur);
    if (element && element->IsLabelableControl()) {
      NS_ASSERTION(cur->IsElement(), "How did that happen?");
      return cur->AsElement();
    }
  }

  return nsnull;
}

