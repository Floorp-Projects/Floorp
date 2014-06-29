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

nsXMLBindingSet::~nsXMLBindingSet()
{}

void
nsXMLBindingSet::AddBinding(nsIAtom* aVar, nsAutoPtr<XPathExpression>&& aExpr)
{
  nsAutoPtr<nsXMLBinding> newbinding(new nsXMLBinding(aVar, Move(aExpr)));

  if (mFirst) {
    nsXMLBinding* binding = mFirst;

    while (binding) {
      // if the target variable is already used in a binding, ignore it
      // since it won't be useful for anything
      if (binding->mVar == aVar)
        return;

      // add the binding at the end of the list
      if (!binding->mNext) {
        binding->mNext = newbinding;
        return;
      }

      binding = binding->mNext;
    }
  }
  else {
    mFirst = newbinding;
  }
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

  nsINode* contextNode = aResult->Node();
  if (!contextNode) {
    return nullptr;
  }

  mValues.EnsureLengthAtLeast(aIndex + 1);

  ErrorResult ignored;
  mValues[aIndex] = aBinding->mExpr->Evaluate(*contextNode, aType, nullptr,
                                              ignored);

  return mValues[aIndex];
}

nsINode*
nsXMLBindingValues::GetNodeAssignmentFor(nsXULTemplateResultXML* aResult,
                                         nsXMLBinding* aBinding,
                                         int32_t aIndex)
{
  XPathResult* result = GetAssignmentFor(aResult, aBinding, aIndex,
                                         XPathResult::FIRST_ORDERED_NODE_TYPE);

  ErrorResult rv;
  return result ? result->GetSingleNodeValue(rv) : nullptr;
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
