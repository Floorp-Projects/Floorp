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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * Implementation of the XForms \<label\> element.
 * This constructs an anonymous \<span\> to hold the inline content.
 */

#include "nsXFormsStubElement.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsControl.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsString.h"

class nsXFormsLabelElement : public nsXFormsXMLVisualStub,
                             public nsIXFormsControl
{
public:
  nsXFormsLabelElement() : mElement(nsnull) { }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aContent);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aPoint);

  // nsIXTFElement overrides
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aNewValue);
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD ChildRemoved(PRUint32 aIndex);

private:
  NS_HIDDEN_(void) RefreshLabel();

  nsIDOMElement *mElement;
  nsCOMPtr<nsIDOMElement> mOuterSpan;
  nsCOMPtr<nsIDOMElement> mInnerSpan;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsLabelElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl)

NS_IMETHODIMP
nsXFormsLabelElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_CHILD_REMOVED);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as wel null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // Create the span that will hold our text.
  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  nsresult rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                                        NS_LITERAL_STRING("span"),
                                        getter_AddRefs(mOuterSpan));

  NS_ENSURE_SUCCESS(rv, rv);

  // Create a text node under the span.
  nsCOMPtr<nsIDOMText> textNode;
  rv = domDoc->CreateTextNode(EmptyString(), getter_AddRefs(textNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> childReturn;
  rv = mOuterSpan->AppendChild(textNode, getter_AddRefs(childReturn));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("span"),
                               getter_AddRefs(mInnerSpan));

  NS_ENSURE_SUCCESS(rv, rv);

  return mOuterSpan->AppendChild(mInnerSpan, getter_AddRefs(childReturn));
}

NS_IMETHODIMP
nsXFormsLabelElement::GetVisualContent(nsIDOMElement **aContent)
{
  NS_ADDREF(*aContent = mOuterSpan);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::GetInsertionPoint(nsIDOMElement **aPoint)
{
  NS_ADDREF(*aPoint = mInnerSpan);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::AttributeSet(nsIAtom *aName, const nsAString &aNewValue)
{
  if (aName == nsXFormsAtoms::ref ||
      aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind) {
    RefreshLabel();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::AttributeRemoved(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::ref ||
      aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind) {
    RefreshLabel();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (aNewParent)
    RefreshLabel();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  RefreshLabel();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildAppended(nsIDOMNode *aChild)
{
  RefreshLabel();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildRemoved(PRUint32 aIndex)
{
  RefreshLabel();
  return NS_OK;
}

void
nsXFormsLabelElement::RefreshLabel()
{
  // The order of precedence for determining the label is:
  //   single node binding, linking, inline text (8.3.3)

  // Because of this, if we have inline text, we must copy it rather than just
  // allowing insertionPoint to point into our anonymous content.  If a binding
  // or linking attributes are present, we don't want to show the inline text
  // at all.

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                       NS_LITERAL_STRING("ref"),
                                       EmptyString(),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));


  nsAutoString labelValue;
  PRBool foundValue = PR_FALSE;

  if (result) {
    nsCOMPtr<nsIDOMNode> singleNode;
    result->GetSingleNodeValue(getter_AddRefs(singleNode));

    if (singleNode) {
      nsXFormsUtils::GetNodeValue(singleNode, labelValue);
      foundValue = PR_TRUE;
    }
  }

  // if (!foundValue) {
  //   TODO: src attribute
  // }

  nsCOMPtr<nsIDOMNode> textNode;
  mOuterSpan->GetFirstChild(getter_AddRefs(textNode));

  if (foundValue) {
    // Hide our inline text and use the given label value.
    textNode->SetNodeValue(labelValue);
    mInnerSpan->SetAttribute(NS_LITERAL_STRING("style"),
                             NS_LITERAL_STRING("display:none"));
  } else {
    // Just show our inline text.
    textNode->SetNodeValue(EmptyString());
    mInnerSpan->RemoveAttribute(NS_LITERAL_STRING("style"));
  }
}

NS_IMETHODIMP
nsXFormsLabelElement::Refresh()
{
  RefreshLabel();
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsLabelElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsLabelElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
