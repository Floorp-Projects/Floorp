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
#include "nsCOMPtr.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIFormControlFrame.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsIEventStateManager.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"

class nsHTMLLabelElement : public nsGenericHTMLFormElement,
                           public nsIDOMHTMLLabelElement
{
public:
  nsHTMLLabelElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLabelElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLLabelElement
  NS_DECL_NSIDOMHTMLLABELELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() const { return NS_FORM_LABEL; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual void SetFocus(nsPresContext* aContext);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  virtual void PerformAccesskey(PRBool aKeyCausesActivation,
                                PRBool aIsTrustedEvent);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  already_AddRefed<nsIContent> GetForContent();
  already_AddRefed<nsIContent> GetFirstFormControl(nsIContent *current);

  // XXX It would be nice if we could use an event flag instead.
  PRBool mHandlingEvent;
};

// construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(Label)


nsHTMLLabelElement::nsHTMLLabelElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo),
    mHandlingEvent(PR_FALSE)
{
}

nsHTMLLabelElement::~nsHTMLLabelElement()
{
}

// nsISupports 


NS_IMPL_ADDREF_INHERITED(nsHTMLLabelElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLabelElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLLabelElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLLabelElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLLabelElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLLabelElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLLabelElement


NS_IMPL_ELEMENT_CLONE(nsHTMLLabelElement)


NS_IMETHODIMP
nsHTMLLabelElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


NS_IMPL_STRING_ATTR(nsHTMLLabelElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLLabelElement, HtmlFor, _for)

nsresult
nsHTMLLabelElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

void
nsHTMLLabelElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (IsInDoc()) {
    RegUnRegAccessKey(PR_FALSE);
  }

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

nsresult
nsHTMLLabelElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (mHandlingEvent ||
      (!NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) &&
       aVisitor.mEvent->message != NS_FOCUS_CONTENT) ||
      aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      !aVisitor.mPresContext) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = GetForContent();
  if (content && !EventTargetIn(aVisitor.mEvent, content, this)) {
    mHandlingEvent = PR_TRUE;
    switch (aVisitor.mEvent->message) {
      case NS_MOUSE_CLICK:
        if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
          if (ShouldFocus(this)) {
            // Focus the for content.
            aVisitor.mPresContext->EventStateManager()->
              ChangeFocusWith(content, nsIEventStateManager::eEventFocusedByKey);
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
                             NS_STATIC_CAST(nsInputEvent*, aVisitor.mEvent),
                             content, PR_FALSE, &status);
          // Do we care about the status this returned?  I don't think we do...
        }
        break;
      case NS_FOCUS_CONTENT:
        // Since we don't have '-moz-user-focus: normal', the only time
        // the event type will be NS_FOCUS_CONTENT will be when the accesskey
        // is activated.  We've already redirected the |SetFocus| call in that
        // case.
        // Since focus doesn't bubble, this is basically the second part
        // of redirecting |SetFocus|.
        {
          nsEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_FOCUS_CONTENT);
          nsEventStatus status = aVisitor.mEventStatus;
          DispatchEvent(aVisitor.mPresContext, &event,
                        content, PR_TRUE, &status);
          // Do we care about the status this returned?  I don't think we do...
        }
        break;
    }
    mHandlingEvent = PR_FALSE;
  }
  return NS_OK;
}

void
nsHTMLLabelElement::SetFocus(nsPresContext* aContext)
{
  // Since we don't have '-moz-user-focus: normal', the only time
  // |SetFocus| will be called is when the accesskey is activated.
  nsCOMPtr<nsIContent> content = GetForContent();
  if (content)
    content->SetFocus(aContext);
}

nsresult
nsHTMLLabelElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLabelElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                      nsIContent* aSubmitElement)
{
  return NS_OK;
}

nsresult
nsHTMLLabelElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                            const nsAString& aValue, PRBool aNotify)
{
  if (aName == nsGkAtoms::accesskey && kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv =
      nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                    aNotify);

  if (aName == nsGkAtoms::accesskey && kNameSpaceID_None == aNameSpaceID &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

nsresult
nsHTMLLabelElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRBool aNotify)
{
  if (aAttribute == nsGkAtoms::accesskey &&
      kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

void
nsHTMLLabelElement::PerformAccesskey(PRBool aKeyCausesActivation,
                                     PRBool aIsTrustedEvent)
{
  if (!aKeyCausesActivation) {
    nsCOMPtr<nsIContent> content = GetForContent();
    if (content)
      content->PerformAccesskey(aKeyCausesActivation, aIsTrustedEvent);
  } else {
    nsPresContext *presContext = GetPresContext();
    if (!presContext)
      return;

    // Click on it if the users prefs indicate to do so.
    nsMouseEvent event(aIsTrustedEvent, NS_MOUSE_CLICK,
                       nsnull, nsMouseEvent::eReal);

    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);

    nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIContent*, this), presContext,
                                &event);
  }
}

inline PRBool IsNonLabelFormControl(nsIContent *aContent)
{
  return aContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
         aContent->Tag() != nsGkAtoms::label;
}

already_AddRefed<nsIContent>
nsHTMLLabelElement::GetForContent()
{
  nsresult rv;

  // Get the element that this label is for
  nsAutoString elementId;
  rv = GetHtmlFor(elementId);
  if (NS_SUCCEEDED(rv) && !elementId.IsEmpty()) {
    // We have a FOR attribute.
    nsCOMPtr<nsIDOMDocument> domDoc;
    GetOwnerDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      nsCOMPtr<nsIDOMElement> domElement;
      domDoc->GetElementById(elementId, getter_AddRefs(domElement));
      nsIContent *result = nsnull;
      if (domElement) {
        CallQueryInterface(domElement, &result);
        if (result && !IsNonLabelFormControl(result)) {
          NS_RELEASE(result); // assigns null
        }
      }
      return result;
    }
  } else {
    // No FOR attribute, we are a label for our first form control element.
    // do a depth-first traversal to look for the first form control element
    return GetFirstFormControl(this);
  }
  return nsnull;
}

already_AddRefed<nsIContent>
nsHTMLLabelElement::GetFirstFormControl(nsIContent *current)
{
  PRUint32 numNodes = current->GetChildCount();

  for (PRUint32 i = 0; i < numNodes; i++) {
    nsIContent *child = current->GetChildAt(i);
    if (child) {
      if (IsNonLabelFormControl(child)) {
        NS_ADDREF(child);
        return child;
      }

      nsIContent* content = GetFirstFormControl(child).get();
      if (content) {
        return content;
      }
    }
  }

  return nsnull;
}
