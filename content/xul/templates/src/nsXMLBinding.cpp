/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateResultXML.h"
#include "nsXMLBinding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/XPathResult.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(nsXMLBindingSet)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(nsXMLBindingSet)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLBindingSet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXMLBindingSet)
  nsXMLBinding* binding = tmp->mFirst;
  while (binding) {
    binding->mExpr = nullptr;
    binding = binding->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXMLBindingSet)
  nsXMLBinding* binding = tmp->mFirst;
  while (binding) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsXMLBinding::mExpr"); 
    cb.NoteXPCOMChild(binding->mExpr);
    binding = binding->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXMLBindingSet, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXMLBindingSet, Release)

nsXMLBindingSet::~nsXMLBindingSet()
{}

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

int32_t
nsXMLBindingSet::LookupTargetIndex(nsIAtom* aTargetVariable,
                                   nsXMLBinding** aBinding)
{
  int32_t idx = 0;
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

XPathResult*
nsXMLBindingValues::GetAssignmentFor(nsXULTemplateResultXML* aResult,
                                     nsXMLBinding* aBinding,
                                     int32_t aIndex,
                                     uint16_t aType)
{
  XPathResult* value = mValues.SafeElementAt(aIndex);
  if (value) {
    return value;
  }

  nsCOMPtr<nsIDOMNode> contextNode;
  aResult->GetNode(getter_AddRefs(contextNode));
  if (!contextNode) {
    return nullptr;
  }

  mValues.EnsureLengthAtLeast(aIndex + 1);

  nsCOMPtr<nsISupports> resultsupports;
  aBinding->mExpr->Evaluate(contextNode, aType,
                            nullptr, getter_AddRefs(resultsupports));

  mValues.ReplaceElementAt(aIndex, XPathResult::FromSupports(resultsupports));

  return mValues[aIndex];
}

void
nsXMLBindingValues::GetNodeAssignmentFor(nsXULTemplateResultXML* aResult,
                                         nsXMLBinding* aBinding,
                                         int32_t aIndex,
                                         nsIDOMNode** aNode)
{
  XPathResult* result = GetAssignmentFor(aResult, aBinding, aIndex,
                                         XPathResult::FIRST_ORDERED_NODE_TYPE);

  nsINode* node;
  ErrorResult rv;
  if (result && (node = result->GetSingleNodeValue(rv))) {
    CallQueryInterface(node, aNode);
  } else {
    *aNode = nullptr;
  }
}

void
nsXMLBindingValues::GetStringAssignmentFor(nsXULTemplateResultXML* aResult,
                                           nsXMLBinding* aBinding,
                                           int32_t aIndex,
                                           nsAString& aValue)
{
  XPathResult* result = GetAssignmentFor(aResult, aBinding, aIndex,
                                         XPathResult::STRING_TYPE);

  if (result) {
    ErrorResult rv;
    result->GetStringValue(aValue, rv);
  } else {
    aValue.Truncate();
  }
}
