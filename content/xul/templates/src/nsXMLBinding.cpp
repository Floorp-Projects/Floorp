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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateResultXML.h"
#include "nsXMLBinding.h"

NS_IMPL_ADDREF(nsXMLBindingSet)
NS_IMPL_RELEASE(nsXMLBindingSet)

nsresult
nsXMLBindingSet::AddBinding(nsIAtom* aVar, nsIDOMXPathExpression* aExpr)
{
  nsAutoPtr<nsXMLBinding> newbinding(new nsXMLBinding(aVar, aExpr));
  NS_ENSURE_TRUE(newbinding, NS_ERROR_OUT_OF_MEMORY);

  if (mFirst) {
    nsXMLBinding* binding = mFirst;

    while (binding) {
      // if the target variable is already used in a binding, ignore it
      // since it won't be useful for anything
      if (binding->mVar == aVar)
        return NS_OK;

      // add the binding at the end of the list
      if (!binding->mNext) {
        binding->mNext = newbinding;
        break;
      }

      binding = binding->mNext;
    }
  }
  else {
    mFirst = newbinding;
  }

  return NS_OK;
}

PRInt32
nsXMLBindingSet::LookupTargetIndex(nsIAtom* aTargetVariable,
                                   nsXMLBinding** aBinding)
{
  PRInt32 idx = 0;
  nsXMLBinding* binding = mFirst;

  while (binding) {
    if (binding->mVar == aTargetVariable) {
      *aBinding = binding;
      return idx;
    }
    idx++;
    binding = binding->mNext;
  }

  *aBinding = nsnull;
  return -1;
}

void
nsXMLBindingValues::GetAssignmentFor(nsXULTemplateResultXML* aResult,
                                     nsXMLBinding* aBinding,
                                     PRInt32 aIndex,
                                     PRUint16 aType,
                                     nsIDOMXPathResult** aValue)
{
  *aValue = mValues.SafeObjectAt(aIndex);

  if (!*aValue) {
    nsCOMPtr<nsIDOMNode> contextNode;
    aResult->GetNode(getter_AddRefs(contextNode));
    if (contextNode) {
      nsCOMPtr<nsISupports> resultsupports;
      aBinding->mExpr->Evaluate(contextNode, aType,
                                nsnull, getter_AddRefs(resultsupports));

      nsCOMPtr<nsIDOMXPathResult> result = do_QueryInterface(resultsupports);
      if (result && mValues.ReplaceObjectAt(result, aIndex))
        *aValue = result;
    }
  }

  NS_IF_ADDREF(*aValue);
}

void
nsXMLBindingValues::GetNodeAssignmentFor(nsXULTemplateResultXML* aResult,
                                         nsXMLBinding* aBinding,
                                         PRInt32 aIndex,
                                         nsIDOMNode** aNode)
{
  nsCOMPtr<nsIDOMXPathResult> result;
  GetAssignmentFor(aResult, aBinding, aIndex,
                   nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                   getter_AddRefs(result));

  if (result)
    result->GetSingleNodeValue(aNode);
  else
    *aNode = nsnull;
}

void
nsXMLBindingValues::GetStringAssignmentFor(nsXULTemplateResultXML* aResult,
                                           nsXMLBinding* aBinding,
                                           PRInt32 aIndex,
                                           nsAString& aValue)
{
  nsCOMPtr<nsIDOMXPathResult> result;
  GetAssignmentFor(aResult, aBinding, aIndex,
                   nsIDOMXPathResult::STRING_TYPE, getter_AddRefs(result));

  if (result)
    result->GetStringValue(aValue);
  else
    aValue.Truncate();
}
