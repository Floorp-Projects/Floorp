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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "nsXFormsActionModuleBase.h"
#include "nsXFormsActionElement.h"
#include "nsIDOM3Node.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMElement.h"
#include "nsIXFormsModelElement.h"
#include "nsXFormsModelElement.h"
#include "nsXFormsUtils.h"

class nsXFormsSetValueElement : public nsXFormsActionModuleBase
{
public:
  nsXFormsSetValueElement();
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT
};

nsXFormsSetValueElement::nsXFormsSetValueElement()
{
}

NS_IMETHODIMP
nsXFormsSetValueElement::HandleAction(nsIDOMEvent* aEvent,
                                      nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;
  
  nsCOMPtr<nsIDOMNode> dommodel = nsXFormsUtils::GetModel(mElement);
  
  if (!dommodel)
    return NS_OK;

  nsAutoString value;
  mElement->GetAttribute(NS_LITERAL_STRING("value"), value);
  if (!value.IsEmpty()) {
    nsCOMPtr<nsIXFormsModelElement> model(do_QueryInterface(dommodel));
    if (!model)
      return NS_OK;

    // Get the instance data and evaluate the xpath expression.
    // XXXfixme when xpath extensions are implemented (instance())

    nsCOMPtr<nsIDOMDocument> instanceDoc;
    model->GetInstanceDocument(NS_LITERAL_STRING(""),
                               getter_AddRefs(instanceDoc));
    if (!instanceDoc)
      return NS_OK;

    nsCOMPtr<nsIDOMElement> docElement;
    instanceDoc->GetDocumentElement(getter_AddRefs(docElement));

    nsCOMPtr<nsIDOMXPathResult> xpResult =
      nsXFormsUtils::EvaluateXPath(value, docElement, mElement,
                                   nsIDOMXPathResult::STRING_TYPE);
    if (!xpResult)
      return NS_OK;
    xpResult->GetStringValue(value);
  }
  else {
    nsCOMPtr<nsIDOM3Node> n3(do_QueryInterface(mElement));
    n3->GetTextContent(value);
  }

  nsCOMPtr<nsIDOMNode> model;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils:: EvaluateNodeBinding(mElement,
                                        nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                        NS_LITERAL_STRING("ref"),
                                        EmptyString(),
                                        nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                        getter_AddRefs(model),
                                        getter_AddRefs(bindElement));

  if (!result)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> singleNode;
  result->GetSingleNodeValue(getter_AddRefs(singleNode));
  if (!singleNode)
    return NS_OK;

  PRUint16 nodeType = 0;
  singleNode->GetNodeType(&nodeType);

  switch (nodeType) {
    case nsIDOMNode::ATTRIBUTE_NODE:
    case nsIDOMNode::TEXT_NODE:
      singleNode->SetNodeValue(value);
      break;
    case nsIDOMNode::ELEMENT_NODE:
        nsCOMPtr<nsIDOM3Node> node = do_QueryInterface(singleNode);
        NS_ASSERTION(node, "DOM Nodes must support DOM3 interfaces");
        node->SetTextContent(value);
        break;
  }
  
  //XXX Mark the node changed
  
  if (aParentAction) {
    aParentAction->SetRecalculate(dommodel, PR_TRUE);
    aParentAction->SetRevalidate(dommodel, PR_TRUE);
    aParentAction->SetRefresh(dommodel, PR_TRUE);
  }
  else {
    nsXFormsUtils::DispatchEvent(dommodel, eEvent_Recalculate);
    nsXFormsUtils::DispatchEvent(dommodel, eEvent_Revalidate);
    nsXFormsUtils::DispatchEvent(dommodel, eEvent_Refresh);
  }
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSetValueElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSetValueElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

