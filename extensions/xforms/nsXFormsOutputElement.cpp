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
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#ifdef DEBUG
//#define DEBUG_XF_OUTPUT
#endif

#include "nsXFormsStubElement.h"
#include "nsIXTFXMLVisualWrapper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathResult.h"
#include "nsISchema.h"

#include "nsIModelElementPrivate.h"
#include "nsIXFormsControl.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"

/**
 * Implementation of the XForms \<output\> element.
 *
 * @see http://www.w3.org/TR/xforms/slice8.html#ui-output
 */
class nsXFormsOutputElement : public nsXFormsXMLVisualStub,
                              public nsIXFormsControl
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  nsXFormsOutputElement() : mElement(nsnull) {}

private:
  nsCOMPtr<nsIDOMElement> mHTMLElement;
  nsIDOMElement *mElement;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsOutputElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsOutputElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_OUTPUT
  printf("nsXFormsOutputElement::OnCreated()\n");
#endif
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_PARENT_CHANGED);

  nsresult rv;

  nsCOMPtr<nsIDOMElement> node;
  rv = aWrapper->GetElementNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = node->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("span"),
                               getter_AddRefs(mHTMLElement));

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsOutputElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsOutputElement::OnDestroyed()
{
  mElement = nsnull;
  mHTMLElement = nsnull;
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::ParentChanged(nsIDOMElement *aNewParent)
{
  // We need to re-evaluate our instance data binding when our parent
  // changes, since xmlns declarations in effect could have changed.
  if (aNewParent) {
    Refresh();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIDOMNode> modelNode = nsXFormsUtils::GetModel(mElement);

    nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);    
    if (model)
      model->RemoveFormControl(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    Refresh();
  }

  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsOutputElement::Refresh()
{
#ifdef DEBUG_XF_OUTPUT
  printf("nsXFormsOutputElement::Refresh()\n");
#endif
  if (!mHTMLElement)
    return NS_OK;

  nsresult rv;
  PRBool hasRef;
  rv = mElement->HasAttribute(NS_LITERAL_STRING("ref"), &hasRef);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result;
  if (hasRef) {
    result = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                                nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                                NS_LITERAL_STRING("ref"),
                                                EmptyString(),
                                                nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                                getter_AddRefs(modelNode),
                                                getter_AddRefs(bindElement));
  } else {
    result = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                                nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                                NS_LITERAL_STRING("value"),
                                                EmptyString(),
                                                nsIDOMXPathResult::STRING_TYPE,
                                                getter_AddRefs(modelNode),
                                                getter_AddRefs(bindElement));
  }

  nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);

  if (model) {
    model->AddFormControl(this);
    if (result) {
      nsAutoString text;

      if (hasRef) {      
        nsCOMPtr<nsIDOMNode> resultNode;
        result->GetSingleNodeValue(getter_AddRefs(resultNode));
        nsXFormsUtils::GetNodeValue(resultNode, text);
      } else {
        rv = result->GetStringValue(text);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      
      nsCOMPtr<nsIDOM3Node> dom3Node = do_QueryInterface(mHTMLElement);
      NS_ENSURE_TRUE(mHTMLElement, NS_ERROR_FAILURE);
      rv = dom3Node->SetTextContent(text);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsOutputElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsOutputElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
