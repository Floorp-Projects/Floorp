/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibleRelation.h"

#include "Relation.h"
#include "xpcAccessibleDocument.h"

#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

nsAccessibleRelation::nsAccessibleRelation(uint32_t aType, Relation* aRel)
    : mType(aType) {
  mTargets = do_CreateInstance(NS_ARRAY_CONTRACTID);
  Accessible* targetAcc = nullptr;
  while ((targetAcc = aRel->Next())) {
    mTargets->AppendElement(static_cast<nsIAccessible*>(ToXPC(targetAcc)));
  }
}

nsAccessibleRelation::~nsAccessibleRelation() {}

// nsISupports
NS_IMPL_ISUPPORTS(nsAccessibleRelation, nsIAccessibleRelation)

// nsIAccessibleRelation
NS_IMETHODIMP
nsAccessibleRelation::GetRelationType(uint32_t* aType) {
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleRelation::GetTargetsCount(uint32_t* aCount) {
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  return mTargets->GetLength(aCount);
}

NS_IMETHODIMP
nsAccessibleRelation::GetTarget(uint32_t aIndex, nsIAccessible** aTarget) {
  NS_ENSURE_ARG_POINTER(aTarget);
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAccessible> target = do_QueryElementAt(mTargets, aIndex, &rv);
  target.forget(aTarget);
  return rv;
}

NS_IMETHODIMP
nsAccessibleRelation::GetTargets(nsIArray** aTargets) {
  NS_ENSURE_ARG_POINTER(aTargets);
  NS_ADDREF(*aTargets = mTargets);
  return NS_OK;
}
