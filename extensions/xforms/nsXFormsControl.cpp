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

#include "nsXFormsControl.h"
#include "nsXFormsModelElement.h"
#include "nsString.h"
#include "nsXFormsAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathNSResolver.h"

NS_IMPL_ISUPPORTS1(nsXFormsControl, nsXFormsControl)

nsXFormsModelElement*
nsXFormsControl::GetModelAndBind(nsIDOMElement **aBindElement)
{
  *aBindElement = nsnull;
  NS_ENSURE_TRUE(mWrapper, nsnull);

  nsCOMPtr<nsIDOMElement> node;
  mWrapper->GetElementNode(getter_AddRefs(node));

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    return nsnull;

  nsAutoString bindId;
  node->GetAttribute(NS_LITERAL_STRING("bind"), bindId);

  nsCOMPtr<nsIDOMNode> modelWrapper;

  if (!bindId.IsEmpty()) {
    // Get the bind element with the given id.
    domDoc->GetElementById(bindId, aBindElement);

    if (*aBindElement) {
      // Walk up the tree looking for the containing model.
      (*aBindElement)->GetParentNode(getter_AddRefs(modelWrapper));

      nsAutoString localName, namespaceURI;
      nsCOMPtr<nsIDOMNode> temp;

      while (modelWrapper) {
        modelWrapper->GetLocalName(localName);
        if (localName.EqualsLiteral("model")) {
          modelWrapper->GetNamespaceURI(namespaceURI);
          if (namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS))
            break;
        }

        temp.swap(modelWrapper);
        temp->GetParentNode(getter_AddRefs(modelWrapper));
      }
    }
  } else {
    // If no bind was given, we use model.
    nsAutoString modelId;
    node->GetAttribute(NS_LITERAL_STRING("model"), modelId);

    if (modelId.IsEmpty()) {
      // No model given, so use the first one in the document.
      nsCOMPtr<nsIDOMNodeList> nodes;
      domDoc->GetElementsByTagNameNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                                     NS_LITERAL_STRING("model"),
                                     getter_AddRefs(nodes));

      if (!nodes)
        return nsnull;

      nodes->Item(0, getter_AddRefs(modelWrapper));
    } else {
      nsCOMPtr<nsIDOMElement> wrapperElement;
      domDoc->GetElementById(modelId, getter_AddRefs(wrapperElement));
      modelWrapper = wrapperElement;
    }
  }

  nsCOMPtr<nsIXTFPrivate> modelPrivate = do_QueryInterface(modelWrapper);
  nsCOMPtr<nsISupports> modelElement;
  if (modelPrivate)
    modelPrivate->GetInner(getter_AddRefs(modelElement));

  nsISupports *modelRaw = NS_STATIC_CAST(nsISupports*, modelElement.get());
  return NS_STATIC_CAST(nsXFormsModelElement*,
                        NS_STATIC_CAST(nsIXFormsModelElement*, modelRaw));
}

already_AddRefed<nsIDOMXPathResult>
nsXFormsControl::EvaluateBinding(PRUint16               aResultType,
                                 nsXFormsModelElement **aModel,
                                 nsIDOMElement        **aBind)
{
  // A control may be attached to a model by either using the 'bind'
  // attribute to give the id of a bind element, or using the 'model'
  // attribute to give the id of a model.  If neither of these are given,
  // the control belongs to the first model in the document.

  *aBind = nsnull;

  NS_IF_ADDREF(*aModel = GetModelAndBind(aBind));
  if (!*aModel)
    return nsnull;

  nsCOMPtr<nsIDOMElement> resolverNode;
  nsAutoString expr;

  if (*aBind) {
    resolverNode = *aBind;
    resolverNode->GetAttribute(NS_LITERAL_STRING("nodeset"), expr);
  } else {
    mWrapper->GetElementNode(getter_AddRefs(resolverNode));
    resolverNode->GetAttribute(NS_LITERAL_STRING("ref"), expr);
  }

  if (expr.IsEmpty())
    return nsnull;

  // Get the instance data and evaluate the xpath expression.
  // XXXfixme when xpath extensions are implemented (instance())
  nsCOMPtr<nsIDOMDocument> instanceDoc;
  (*aModel)->GetInstanceDocument(NS_LITERAL_STRING(""),
                                 getter_AddRefs(instanceDoc));

  if (!instanceDoc)
    return nsnull;

  nsCOMPtr<nsIDOMXPathEvaluator> eval = do_QueryInterface(instanceDoc);
  nsCOMPtr<nsIDOMXPathNSResolver> resolver;
  eval->CreateNSResolver(resolverNode, getter_AddRefs(resolver));
  NS_ENSURE_TRUE(resolver, nsnull);

  nsCOMPtr<nsIDOMElement> docElement;
  instanceDoc->GetDocumentElement(getter_AddRefs(docElement));
  if (!docElement)
    return nsnull;   // this will happen if the doc is still loading

  nsCOMPtr<nsISupports> result;
  eval->Evaluate(expr, docElement, resolver, aResultType, nsnull,
                 getter_AddRefs(result));

  nsIDOMXPathResult *xpResult = nsnull;
  if (result)
    CallQueryInterface(result, &xpResult); // addrefs

  return xpResult;
}
