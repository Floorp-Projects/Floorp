/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateResultXML.h"
#include "nsXMLBinding.h"

NS_IMPL_ADDREF(nsXMLBindingSet)
NS_IMPL_RELEASE(nsXMLBindingSet)

NS_IMPL_CYCLE_COLLECTION_NATIVE_CLASS(nsXMLBindingSet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsXMLBindingSet)
  nsXMLBinding* binding = tmp->mFirst;
  while (binding) {
    binding->mExpr = nullptr;
    binding = binding->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsXMLBindingSet)
  nsXMLBinding* binding = tmp->mFirst;
  while (binding) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsXMLBinding::mExpr"); 
    cb.NoteXPCOMChild(binding->mExpr);
    binding = binding->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXMLBindingSet, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXMLBindingSet, Release)

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

  *aBinding = nullptr;
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
                                nullptr, getter_AddRefs(resultsupports));

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
    *aNode = nullptr;
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
