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
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMHTMLDivElement.h"

#include "nsIXTFXMLVisual.h"
#include "nsIXTFXMLVisualWrapper.h"

#include "nsXFormsControlStub.h"
#include "nsIXFormsContextControl.h"
#include "nsIModelElementPrivate.h"
#include "nsXFormsUtils.h"

#ifdef DEBUG
// #define DEBUG_XF_GROUP
#endif

/**
 * Implementation of the XForms \<group\> control.
 * 
 * @see http://www.w3.org/TR/xforms/slice9.html#id2631290
 *
 * @todo If a \<label\> is the first element child for \<group\> it is the
 * label for the entire group
 *
 * @todo "Setting the input focus on a group results in the focus being set to
 * the first form control in the navigation order within that group."
 * (spec. 9.1.1)
 *
 * @bug If a group only has a model attribute, the group fails to set this for
 * children, as it is impossible to distinguish between a failure and absence
 * of binding attributes when calling ProcessNodeBinding().
 */
class nsXFormsGroupElement : public nsXFormsControlStub,
                             public nsIXFormsContextControl
{
protected:
  /** Tries to focus a child form control.*/
  PRBool TryFocusChildControl(nsIDOMNode* aParent);
  
  /** The UI HTML element used to represent the tag */
  nsCOMPtr<nsIDOMHTMLDivElement> mHTMLElement;

  /** The current ID of the model node is bound to */
  nsString mModelID;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // Constructor
  nsXFormsGroupElement();
  ~nsXFormsGroupElement();

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);
  
  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();

  // nsIXFormsControl
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);

  // nsIXFormsContextControl
  NS_DECL_NSIXFORMSCONTEXTCONTROL
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsGroupElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIXFormsContextControl)

MOZ_DECL_CTOR_COUNTER(nsXFormsGroupElement)

nsXFormsGroupElement::nsXFormsGroupElement()
{
  MOZ_COUNT_CTOR(nsXFormsGroupElement);
}

nsXFormsGroupElement::~nsXFormsGroupElement()
{
  MOZ_COUNT_DTOR(nsXFormsGroupElement);
}

// nsIXTFXMLVisual
NS_IMETHODIMP
nsXFormsGroupElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::OnCreated(aWrapper=%p)\n", (void*) aWrapper);
#endif

  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create HTML tag
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMElement> domElement;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("div"),
                          getter_AddRefs(domElement));

  mHTMLElement = do_QueryInterface(domElement);
  NS_ENSURE_TRUE(mHTMLElement, NS_ERROR_FAILURE);
  
  return NS_OK;
}

// nsIXTFVisual
NS_IMETHODIMP
nsXFormsGroupElement::GetVisualContent(nsIDOMElement * *aVisualContent)
{
  NS_ADDREF(*aVisualContent = mHTMLElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsGroupElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

// nsIXTFElement
NS_IMETHODIMP
nsXFormsGroupElement::OnDestroyed()
{
  mHTMLElement = nsnull;
  return nsXFormsControlStub::OnDestroyed();
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsGroupElement::Refresh()
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::Refresh()\n");
#endif

  if (!mHTMLElement) 
    return NS_OK;

  mModelID.Truncate();

  nsCOMPtr<nsIModelElementPrivate> modelNode;
  nsCOMPtr<nsIDOMXPathResult> result;
  nsresult rv =
    ProcessNodeBinding(NS_LITERAL_STRING("ref"),
                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                       getter_AddRefs(result),
                       getter_AddRefs(modelNode));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!result) {
    return NS_OK;
  }

  
  // Get model ID
  nsCOMPtr<nsIDOMElement> modelElement = do_QueryInterface(modelNode);
  NS_ENSURE_TRUE(modelElement, NS_ERROR_FAILURE);
  modelElement->GetAttribute(NS_LITERAL_STRING("id"), mModelID);

  // Get context node, if any  
  result->GetSingleNodeValue(getter_AddRefs(mBoundNode));
  NS_ENSURE_STATE(mBoundNode);
  
  return NS_OK;
}

PRBool
nsXFormsGroupElement::TryFocusChildControl(nsIDOMNode* aParent)
{
  if (!aParent)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = aParent->GetChildNodes(getter_AddRefs(children));
  if (NS_FAILED(rv))
    return PR_FALSE;

  PRUint32 childCount = 0;
  children->GetLength(&childCount);
  nsCOMPtr<nsIDOMNode> child;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIXFormsControl> control(do_QueryInterface(child));
    PRBool focus = PR_FALSE;
    if (control) {
      control->TryFocus(&focus);
      if (focus)
        return PR_TRUE;
    }
    focus = TryFocusChildControl(child);
    if (focus)
      return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsXFormsGroupElement::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  if (GetRelevantState()) {
    *aOK = TryFocusChildControl(mElement);
  }
  return NS_OK;
}

// nsIXFormsContextControl
NS_IMETHODIMP
nsXFormsGroupElement::SetContextNode(nsIDOMNode *aContextNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXFormsGroupElement::GetContext(nsAString&      aModelID,
                                 nsIDOMNode    **aContextNode,
                                 PRInt32        *aContextPosition,
                                 PRInt32        *aContextSize)
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::GetContext()\n");
#endif
  NS_ENSURE_ARG(aContextSize);
  NS_ENSURE_ARG(aContextPosition);

  *aContextPosition = 1;
  *aContextSize = 1;

  if (mBoundNode && aContextNode)
    CallQueryInterface(mBoundNode, aContextNode); // addrefs
  aModelID = mModelID;
  
  return NS_OK;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsGroupElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsGroupElement();
  if (!*aResult)
  return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
