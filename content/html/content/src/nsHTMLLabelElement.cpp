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
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
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
  NS_IMETHOD GetTagName(nsAWritableString& aTagName) {
    return nsGenericHTMLContainerFormElement::GetTagName(aTagName);
  }
  NS_IMETHOD GetAttribute(const nsAReadableString& aName,
                          nsAWritableString& aReturn) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerFormElement::GetAttribute(NS_LITERAL_STRING("for"), aReturn);
    }
    return nsGenericHTMLContainerFormElement::GetAttribute(aName, aReturn);
  }
  NS_IMETHOD SetAttribute(const nsAReadableString& aName,
                          const nsAReadableString& aValue) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerElement::SetAttribute(NS_LITERAL_STRING("for"), aValue);
    }
    return nsGenericHTMLContainerElement::SetAttribute(aName, aValue);
  }
  NS_IMETHOD RemoveAttribute(const nsAReadableString& aName) {
    nsAutoString name(aName);
    if (name.EqualsIgnoreCase("htmlfor")) {
      return nsGenericHTMLContainerFormElement::RemoveAttribute(NS_LITERAL_STRING("for"));
    }
    return nsGenericHTMLContainerFormElement::RemoveAttribute(aName);
  }
  NS_IMETHOD GetAttributeNode(const nsAReadableString& aName,
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
  NS_IMETHOD GetElementsByTagName(const nsAReadableString& aTagname,
                                  nsIDOMNodeList** aReturn) {
    return nsGenericHTMLContainerFormElement::GetElementsByTagName(aTagname,
                                                                   aReturn);
  }
  NS_IMETHOD GetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
                            nsAWritableString& aReturn) {
    return nsGenericHTMLContainerFormElement::GetAttributeNS(aNamespaceURI,
                                                             aLocalName,
                                                             aReturn);
  }
  NS_IMETHOD SetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aQualifiedName,
                            const nsAReadableString& aValue) {
    return nsGenericHTMLContainerFormElement::SetAttributeNS(aNamespaceURI,
                                                             aQualifiedName,
                                                             aValue);
  }
  NS_IMETHOD RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName) {
    return nsGenericHTMLContainerFormElement::RemoveAttributeNS(aNamespaceURI,
                                                                aLocalName);
  }
  NS_IMETHOD GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName,
                                nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::GetAttributeNodeNS(aNamespaceURI,
                                                                 aLocalName,
                                                                 aReturn);
  }
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLContainerFormElement::SetAttributeNodeNS(aNewAttr,
                                                                 aReturn);
  }
  NS_IMETHOD GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                    const nsAReadableString& aLocalName,
                                    nsIDOMNodeList** aReturn) {
    return nsGenericHTMLContainerFormElement::GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn);
  }
  NS_IMETHOD HasAttribute(const nsAReadableString& aName, PRBool* aReturn) {
    return HasAttribute(aName, aReturn);
  }
  NS_IMETHOD HasAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
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
  NS_IMETHOD GetType(PRInt32* aType);

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
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
{
}

nsHTMLLabelElement::~nsHTMLLabelElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
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


// nsIFormControl

NS_IMETHODIMP
nsHTMLLabelElement::GetType(PRInt32* aType)
{
  *aType = NS_FORM_LABEL;

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLLabelElement, AccessKey, accesskey)
//NS_IMPL_STRING_ATTR(nsHTMLLabelElement, HtmlFor, _for)

NS_IMETHODIMP
nsHTMLLabelElement::GetHtmlFor(nsAWritableString& aValue)
{
  nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                             nsHTMLAtoms::_for, aValue);
  return NS_OK;                                                    
}  

NS_IMETHODIMP
nsHTMLLabelElement::SetHtmlFor(const nsAReadableString& aValue)
{
  // trim leading and trailing whitespace 
  static char whitespace[] = " \r\n\t";
  nsAutoString value(aValue);
  value.Trim(whitespace, PR_TRUE, PR_TRUE);
  return nsGenericHTMLContainerFormElement::SetAttr(kNameSpaceID_HTML,
                                                    nsHTMLAtoms::_for,
                                                    value, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLLabelElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  nsresult rv;
  rv = nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext,
                                                         aEvent,
                                                         aDOMEvent,
                                                         aFlags,
                                                         aEventStatus);

  // Now a little special trickery because we are a label:
  // We need to pass this event on to our child iff it is a focus,
  // keypress/up/dn, mouseclick/dblclick/up/down.
  if ((NS_OK == rv) && (NS_EVENT_FLAG_INIT & aFlags) &&
      ((nsEventStatus_eIgnore == *aEventStatus) ||
       (nsEventStatus_eConsumeNoDefault == *aEventStatus)) ) {
    PRBool isFormElement = PR_FALSE;
    nsCOMPtr<nsIHTMLContent> node; // Node we are a label for
    switch (aEvent->message) {
      case NS_FOCUS_CONTENT:

// Bug 49897: According to the spec, the following should not be passed
// Bug 7554: Despite the spec, IE passes left click events, so for
// compatability:
      case NS_MOUSE_LEFT_CLICK:
//      case NS_MOUSE_LEFT_DOUBLECLICK:
//      case NS_MOUSE_LEFT_BUTTON_UP:
//      case NS_MOUSE_LEFT_BUTTON_DOWN:
//      case NS_MOUSE_MIDDLE_CLICK:
//      case NS_MOUSE_MIDDLE_DOUBLECLICK:
//      case NS_MOUSE_MIDDLE_BUTTON_UP:
//      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
//      case NS_MOUSE_RIGHT_CLICK:
//      case NS_MOUSE_RIGHT_DOUBLECLICK:
//      case NS_MOUSE_RIGHT_BUTTON_UP:
//      case NS_MOUSE_RIGHT_BUTTON_DOWN:
//      case NS_KEY_PRESS:
//      case NS_KEY_UP:
//      case NS_KEY_DOWN:
      {
        // Get the element that this label is for
        nsAutoString elementId;
        rv = GetHtmlFor(elementId);
        if (NS_SUCCEEDED(rv) && elementId.Length()) { // --- We have a FOR attr
          nsCOMPtr<nsIDocument> iDoc;
          rv = GetDocument(*getter_AddRefs(iDoc));
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIDOMElement> domElement;

            nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(iDoc));

            if (domDoc) {
              rv = domDoc->GetElementById(elementId,
                                          getter_AddRefs(domElement));
            }

            if (NS_SUCCEEDED(rv) && domElement) {
              // Get our grubby paws on the content interface
              node = do_QueryInterface(domElement, &rv);

              // Find out of this is a form element.
              if (NS_SUCCEEDED(rv)) {
                nsIFormControlFrame* control;
                nsresult gotFrame = GetPrimaryFrame(node, control);
                isFormElement = NS_SUCCEEDED(gotFrame) && control;
              }
            }
          }
        } else {
          // --- No FOR attribute, we are a label for our first child
          // element
          PRInt32 numNodes;
          rv = ChildCount(numNodes);
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIContent> contNode;
            PRInt32 i;
            for (i = 0; NS_SUCCEEDED(rv) && !isFormElement && (i < numNodes);
                 i++) {
              rv = ChildAt(i, *getter_AddRefs(contNode));

              if (NS_SUCCEEDED(rv) && contNode) {
                // We need to make sure this child is a form element
                nsresult isHTMLContent = PR_FALSE;

                node = do_QueryInterface(contNode, &isHTMLContent);

                if (NS_SUCCEEDED(isHTMLContent) && node) {
                  // Find out of this is a form element.
                  nsIFormControlFrame* control;

                  nsresult gotFrame = GetPrimaryFrame(node, control);

                  isFormElement = NS_SUCCEEDED(gotFrame) && control;
                }
              }
            }
          }
        }
      } // Close should handle
    } // Close switch

    // If we found an element, pass along the event to it.
    if (NS_SUCCEEDED(rv) && node) {
      // Only pass along event if this is a form element
      if (isFormElement) {
        rv = node->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags,
                                  aEventStatus);
      }
    }
  } // Close trickery

  return rv;
}

NS_IMETHODIMP
nsHTMLLabelElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
