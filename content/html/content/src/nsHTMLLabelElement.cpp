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
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIFormControlFrame.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsIEventStateManager.h"


class nsHTMLLabelElement : public nsGenericHTMLFormElement,
                           public nsIDOMHTMLLabelElement
{
public:
  nsHTMLLabelElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLabelElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLLabelElement
  NS_DECL_NSIDOMHTMLLABELELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() { return NS_FORM_LABEL; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);

  // nsIContent
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual void SetFocus(nsIPresContext* aContext);
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


NS_IMPL_HTML_DOM_CLONENODE(Label)


NS_IMETHODIMP
nsHTMLLabelElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


NS_IMPL_STRING_ATTR(nsHTMLLabelElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLLabelElement, HtmlFor, _for)

void
nsHTMLLabelElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  PRBool documentChanging = (aDocument != mDocument);

  // Unregister the access key for the old document.
  if (documentChanging && mDocument) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsGenericHTMLFormElement::SetDocument(aDocument, aDeep,
                                        aCompileEventHandlers);

  // Register the access key for the new document.
  if (documentChanging && mDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }
}

static PRBool
EventTargetIn(nsIPresContext *aPresContext, nsEvent *aEvent,
              nsIContent *aChild, nsIContent *aStop)
{
  nsCOMPtr<nsIContent> c;
  aPresContext->EventStateManager()->GetEventTargetContent(aEvent,
                                                           getter_AddRefs(c));
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
nsHTMLLabelElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  nsresult rv = nsGenericHTMLFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                         aDOMEvent, aFlags,
                                                         aEventStatus);
  if (NS_FAILED(rv))
    return rv;

  if (mHandlingEvent ||
      *aEventStatus == nsEventStatus_eConsumeNoDefault ||
      (aEvent->message != NS_MOUSE_LEFT_CLICK &&
       aEvent->message != NS_FOCUS_CONTENT) ||
      aFlags & NS_EVENT_FLAG_CAPTURE)
    return NS_OK;

  nsCOMPtr<nsIContent> content = GetForContent();
  if (content && !EventTargetIn(aPresContext, aEvent, content, this)) {
    mHandlingEvent = PR_TRUE;
    switch (aEvent->message) {
      case NS_MOUSE_LEFT_CLICK:
        // Focus the for content.
        content->SetFocus(aPresContext);
        // This sends the event twice down parts of its path.  Oh well.
        // This is needed for:
        //  * Making radio buttons and checkboxes get checked.
        //  * Triggering user event handlers. (For compatibility with IE,
        //    we do only left click.  If we wanted to interpret the HTML
        //    spec very narrowly, we would do nothing.  If we wanted to
        //    do something sensible, we might send more events through
        //    like this.)  See bug 7554, bug 49897, and bug 96813.
        // XXX The event should probably have its target modified.  See
        // bug 146066.  (But what if |aDOMEvent| is null and it gets
        // created later?  If we forced the existence of an event and
        // modified its target, we could replace |mHandlingEvent|.)
        rv = content->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                     aFlags, aEventStatus);
        break;
      case NS_FOCUS_CONTENT:
        // Since we don't have '-moz-user-focus: normal', the only time
        // the event type will be NS_FOCUS_CONTENT will be when the accesskey
        // is activated.  We've already redirected the |SetFocus| call in that
        // case.
        // Since focus doesn't bubble, this is basically the second part
        // of redirecting |SetFocus|.
        rv = content->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                     aFlags, aEventStatus);
        break;
    }
    mHandlingEvent = PR_FALSE;
  }
  return rv;
}

void
nsHTMLLabelElement::SetFocus(nsIPresContext* aContext)
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
  if (aName == nsHTMLAtoms::accesskey && kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv =
      nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                    aNotify);

  if (aName == nsHTMLAtoms::accesskey && kNameSpaceID_None == aNameSpaceID &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

nsresult
nsHTMLLabelElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::accesskey &&
      kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

inline PRBool IsNonLabelFormControl(nsIContent *aContent)
{
  return aContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL) &&
         aContent->Tag() != nsHTMLAtoms::label;
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
