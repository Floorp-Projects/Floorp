/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccessibleRelation_H_
#define _nsAccessibleRelation_H_

#include "nsIAccessibleRelation.h"

#include "nsCOMPtr.h"
#include "nsIMutableArray.h"

class Relation;

/**
 * Class represents an accessible relation.
 */
class nsAccessibleRelation : public nsIAccessibleRelation
{
public:
  nsAccessibleRelation(PRUint32 aType, Relation* aRel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLERELATION

private:
  nsAccessibleRelation();
  nsAccessibleRelation(const nsAccessibleRelation&);
  nsAccessibleRelation& operator = (const nsAccessibleRelation&);
  
  PRUint32 mType;
  nsCOMPtr<nsIMutableArray> mTargets;
};

#endif
