/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsISizeOfHandler.h"
#include "nsIFormControlFrame.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsIEventStateManager.h"


class nsHTMLLabelElement : public nsGenericHTMLContainerFormElement,
                           public nsIDOMHTMLLabelElement
{
public:
  nsHTMLLabelElement();
  virtual ~nsHTMLLabelElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerFormElement::)

  // nsIDOMElement, because of the "htmlFor" attribute handling we can't
  // use the NS_FORWARD_NSIDOMHTMLELEMENT macro here...
  NS_IMETHOD GetTagName(nsAString& aTagName) {
    return nsGenericHTMLContainerFormElement::GetTagName(aTagName);
  }
  NS_IMETHOD GetAttribute(const nsAString& aName,
                          nsAString& aReturn) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerFormElement::GetAttribute(NS_LITERAL_STRING("for"), aReturn);
    }
    return nsGenericHTMLContainerFormElement::GetAttribute(aName, aReturn);
  }
  NS_IMETHOD SetAttribute(const nsAString& aName,
                          const nsAString& aValue) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerElement::SetAttribute(NS_LITERAL_STRING("for"), aValue);
    }
    return nsGenericHTMLContainerElement::SetAttribute(aName, aValue);
  }
  NS_IMETHOD RemoveAttribute(const nsAString& aName) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerFormElement::RemoveAttribute(NS_LITERAL_STRING("for"));
    }
    return nsGenericHTMLContainerFormElement::RemoveAttribute(aName);
  }
  NS_IMETHOD GetAttributeNode(const nsAString& aName,
                              nsIDOMAttr** aReturn) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerFormElement::GetAttributeNode(NS_LITERAL_STRING("for"), aReturn);
    }
    return nsGenericHTMLContainerFormElement::GetAttributeNode(aName, aReturn);
  }
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::SetAttributeNode(aNewAttr, aReturn);
  }
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::RemoveAttributeNode(aOldAttr, aReturn);
  }
  NS_IMETHOD GetElementsByTagName(const nsAString& aTagname,
                                  nsIDOMNodeList** aReturn) {
    return nsGenericHTMLContainerFormElement::GetElementsByTagName(aTagname,
                                                                   aReturn);
  }
  NS_IMETHOD GetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            nsAString& aReturn) {
    return nsGenericHTMLContainerFormElement::GetAttributeNS(aNamespaceURI,
                                                             aLocalName,
                                                             aReturn);
  }
  NS_IMETHOD SetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            const nsAString& aValue) {
    return nsGenericHTMLContainerFormElement::SetAttributeNS(aNamespaceURI,
                                                             aQualifiedName,
                                                             aValue);
  }
  NS_IMETHOD RemoveAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aLocalName) {
    return nsGenericHTMLContainerFormElement::RemoveAttributeNS(aNamespaceURI,
                                                                aLocalName);
  }
  NS_IMETHOD GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                const nsAString& aLocalName,
                                nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::GetAttributeNodeNS(aNamespaceURI,
                                                                 aLocalName,
                                                                 aReturn);
  }
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::SetAttributeNodeNS(aNewAttr,
                                                                 aReturn);
  }
  NS_IMETHOD GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName,
                                    nsIDOMNodeList** aReturn) {
    return nsGenericHTMLContainerFormElement::GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn);
  }
  NS_IMETHOD HasAttribute(const nsAString& aName, PRBool* aReturn) {
    return nsGenericHTMLContainerFormElement::HasAttribute(aName, aReturn);
  }
  NS_IMETHOD HasAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            PRBool* aReturn) {
    return nsGenericHTMLContainerFormElement::HasAttributeNS(aNamespaceURI,
                                                             aLocalName,
                                                             aReturn);
  }

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerFormElement::)

  // nsIDOMHTMLLabelElement
  NS_DECL_NSIDOMHTMLLABELELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() { return NS_FORM_LABEL; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);

  // nsIContent
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aContext);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue, PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo, const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                       PRBool aNotify);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  already_AddRefed<nsIContent> GetForContent();

  // XXX It would be nice if we could use an event flag instead.
  PRBool mHandlingEvent;
};

// construction, destruction
nsresult
NS_NewHTMLLabelElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLLabelElement* it = new nsHTMLLabelElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLLabelElement::nsHTMLLabelElement()
  : mHandlingEvent(PR_FALSE)
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
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLLabelElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLLabelElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLLabelElement

nsresult
nsHTMLLabelElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLLabelElement* it = new nsHTMLLabelElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLabelElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
}


NS_IMPL_STRING_ATTR(nsHTMLLabelElement, AccessKey, accesskey)
//NS_IMPL_STRING_ATTR(nsHTMLLabelElement, HtmlFor, _for)

NS_IMETHODIMP
nsHTMLLabelElement::GetHtmlFor(nsAString& aValue)
{
  nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_None,
                                             nsHTMLAtoms::_for, aValue);
  return NS_OK;                                                    
}  

NS_IMETHODIMP
nsHTMLLabelElement::SetHtmlFor(const nsAString& aValue)
{
  // trim leading and trailing whitespace 
  static char whitespace[] = " \r\n\t";
  nsAutoString value(aValue);
  value.Trim(whitespace, PR_TRUE, PR_TRUE);
  return nsGenericHTMLContainerFormElement::SetAttr(kNameSpaceID_None,
                                                    nsHTMLAtoms::_for,
                                                    value, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLLabelElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  // Unregister the access key for the old document.
  if (mDocument) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv = nsGenericHTMLContainerFormElement::SetDocument(aDocument,
                                                 aDeep, aCompileEventHandlers);

  // Register the access key for the new document.
  if (mDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

static PRBool
EventTargetIn(nsIPresContext *aPresContext, nsEvent *aEvent,
              nsIContent *aChild, nsIContent *aStop)
{
  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));
  nsCOMPtr<nsIContent> c;
  esm->GetEventTargetContent(aEvent, getter_AddRefs(c));
  while (c) {
    if (c == aChild) {
      return PR_TRUE;
    }
    if (c == aStop) {
      break;
    }
    nsIContent *parent;
    c->GetParent(parent);
    c = dont_AddRef(parent);
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLLabelElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  nsresult rv = nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext,
                                      aEvent, aDOMEvent, aFlags, aEventStatus);
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
        rv = content->SetFocus(aPresContext);
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

NS_IMETHODIMP
nsHTMLLabelElement::SetFocus(nsIPresContext* aContext)
{
  // Since we don't have '-moz-user-focus: normal', the only time
  // |SetFocus| will be called is when the accesskey is activated.
  nsCOMPtr<nsIContent> content = GetForContent();
  if (content)
    return content->SetFocus(aContext);

  // Do nothing (yes, really)!
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLLabelElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

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

NS_IMETHODIMP
nsHTMLLabelElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            const nsAString& aValue, PRBool aNotify)
{
  if (aName == nsHTMLAtoms::accesskey && kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv =
      nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aValue, aNotify);

  if (aName == nsHTMLAtoms::accesskey && kNameSpaceID_None == aNameSpaceID &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLLabelElement::SetAttr(nsINodeInfo* aNodeInfo, const nsAString& aValue,
                            PRBool aNotify)
{
  return nsGenericHTMLElement::SetAttr(aNodeInfo, aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLLabelElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::accesskey &&
      kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
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
        if (result && !result->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
          NS_RELEASE(result); // assigns null
        }
      }
      return result;
    }
  } else {
    // No FOR attribute, we are a label for our first child element.
    PRInt32 numNodes;
    rv = ChildCount(numNodes);
    if (NS_SUCCEEDED(rv)) {
      for (PRInt32 i = 0; i < numNodes; i++) {
        nsIContent *result;
        ChildAt(i, result);
        if (result) {
          if (result->IsContentOfType(nsIContent::eHTML_FORM_CONTROL))
            return result;
          NS_RELEASE(result);
        }
      }
    }
  }
  return nsnull;
}
