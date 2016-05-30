/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccessibleRelation_H_
#define _nsAccessibleRelation_H_

#include "nsIAccessibleRelation.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/a11y/ProxyAccessible.h"

namespace mozilla {
namespace a11y {

class Relation;

/**
 * Class represents an accessible relation.
 */
class nsAccessibleRelation final : public nsIAccessibleRelation
{
public:
  nsAccessibleRelation(uint32_t aType, Relation* aRel);

  nsAccessibleRelation(uint32_t aType,
                       const nsTArray<ProxyAccessible*>* aTargets);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLERELATION

private:
  nsAccessibleRelation();
  ~nsAccessibleRelation();

  nsAccessibleRelation(const nsAccessibleRelation&);
  nsAccessibleRelation& operator = (const nsAccessibleRelation&);

  uint32_t mType;
  nsCOMPtr<nsIMutableArray> mTargets;
};

} // namespace a11y
} // namespace mozilla

#endif
