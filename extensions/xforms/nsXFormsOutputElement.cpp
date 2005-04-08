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
#include "nsXFormsControlStub.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"

/**
 * Implementation of the XForms \<output\> element.
 *
 * @see http://www.w3.org/TR/xforms/slice8.html#ui-output
 */
class nsXFormsOutputElement : public nsXFormsControlStub
{
public:
  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);

  // nsIXFormsControl
  NS_IMETHOD Bind();
  NS_IMETHOD Refresh();

private:
  nsCOMPtr<nsIDOMElement> mLabel;
  nsCOMPtr<nsIDOMElement> mContainer;
  nsCOMPtr<nsIDOMElement> mValue;
  PRBool                  mHasBinding;
};

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsOutputElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_OUTPUT
  printf("nsXFormsOutputElement::OnCreated()\n");
#endif

  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  /* input uses <html:label /> as the container, but output does't need the
     additional features html:label gives, so just use a html:span.
  */

  // Our anonymous content structure will look like this:
  //
  // <span>                        (mContainer)
  //   <span/>                     (mLabel)
  //   <span/>                     (mValue)
  // </span>

  nsCOMPtr<nsIDOMNode> childReturn;

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(mContainer));
  NS_ENSURE_STATE(mContainer);

  rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("span"),
                               getter_AddRefs(mLabel));
  NS_ENSURE_STATE(mLabel);

  mContainer->AppendChild(mLabel, getter_AddRefs(childReturn));

  rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                               NS_LITERAL_STRING("span"),
                               getter_AddRefs(mValue));
  NS_ENSURE_STATE(mValue);

  mContainer->AppendChild(mValue, getter_AddRefs(childReturn));

  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsOutputElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  nsCOMPtr<nsIDOMNode> childNode;
  mContainer->GetFirstChild(getter_AddRefs(childNode));
  return CallQueryInterface(childNode, aElement);
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsOutputElement::OnDestroyed()
{
  mLabel = nsnull;
  mContainer = nsnull;
  mValue = nsnull;

  return nsXFormsControlStub::OnDestroyed();
}

NS_IMETHODIMP
nsXFormsOutputElement::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref ||
      aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::value) {
    if (mModel)
      mModel->RemoveFormControl(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref ||
      aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::value) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

// nsIXFormsControl

nsresult
nsXFormsOutputElement::Bind()
{
  if (!mValue || !mHasParent)
    return NS_OK;
  
  mBoundNode = nsnull;

  nsresult rv;
  rv = mElement->HasAttribute(NS_LITERAL_STRING("ref"), &mHasBinding);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!mHasBinding) {
    rv = mElement->HasAttribute(NS_LITERAL_STRING("bind"), &mHasBinding);
    NS_ENSURE_SUCCESS(rv, rv);
  }

    nsCOMPtr<nsIDOMXPathResult> result;
  if (mHasBinding) {
    rv = ProcessNodeBinding(NS_LITERAL_STRING("ref"),
                            nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                            getter_AddRefs(result));
  } else {
    rv = ProcessNodeBinding(NS_LITERAL_STRING("value"),
                            nsIDOMXPathResult::STRING_TYPE,
                            getter_AddRefs(result));
  }
  
  NS_ENSURE_SUCCESS(rv, rv);

  if (result) {
    result->GetSingleNodeValue(getter_AddRefs(mBoundNode));
  }

  return NS_OK;
}
  
NS_IMETHODIMP
nsXFormsOutputElement::Refresh()
{
  if (!mValue || !mHasParent)
    return NS_OK;

  nsAutoString text;    
  nsresult rv;
  if (mHasBinding) {
    if (mBoundNode) {
      nsXFormsUtils::GetNodeValue(mBoundNode, text);
    }
  } else {
    ///
    /// @todo Update mBoundNode? (XXX)
    nsCOMPtr<nsIDOMXPathResult> result;
    rv = ProcessNodeBinding(NS_LITERAL_STRING("value"),
                            nsIDOMXPathResult::STRING_TYPE,
                            getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    if (result) {
      rv = result->GetStringValue(text);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<nsIDOM3Node> dom3Node = do_QueryInterface(mValue);
  NS_ENSURE_STATE(dom3Node);

  rv = dom3Node->SetTextContent(text);

  return rv;
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
