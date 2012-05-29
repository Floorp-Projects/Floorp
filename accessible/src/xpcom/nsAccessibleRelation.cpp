/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibleRelation.h"

#include "Relation.h"
#include "Accessible.h"

#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"

nsAccessibleRelation::nsAccessibleRelation(PRUint32 aType,
                                           Relation* aRel) :
  mType(aType)
{
  mTargets = do_CreateInstance(NS_ARRAY_CONTRACTID);
  nsIAccessible* targetAcc = nsnull;
  while ((targetAcc = aRel->Next()))
    mTargets->AppendElement(targetAcc, false);
}

// nsISupports
NS_IMPL_ISUPPORTS1(nsAccessibleRelation, nsIAccessibleRelation)

// nsIAccessibleRelation
NS_IMETHODIMP
nsAccessibleRelation::GetRelationType(PRUint32 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleRelation::GetTargetsCount(PRUint32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  return mTargets->GetLength(aCount);
}

NS_IMETHODIMP
nsAccessibleRelation::GetTarget(PRUint32 aIndex, nsIAccessible **aTarget)
{
  NS_ENSURE_ARG_POINTER(aTarget);
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAccessible> target = do_QueryElementAt(mTargets, aIndex, &rv);
  target.forget(aTarget);
  return rv;
}

NS_IMETHODIMP
nsAccessibleRelation::GetTargets(nsIArray **aTargets)
{
  NS_ENSURE_ARG_POINTER(aTargets);
  NS_ADDREF(*aTargets = mTargets);
  return NS_OK;
}
