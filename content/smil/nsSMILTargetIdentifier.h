/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTARGETIDENTIFIER_H_
#define NS_SMILTARGETIDENTIFIER_H_

#include "mozilla/dom/Element.h"
#include "nsAutoPtr.h"
#include "prtypes.h"

/**
 * Struct: nsSMILTargetIdentifier
 *
 * Tuple of: { Animated Element, Attribute Name, Attribute Type (CSS vs. XML) }
 *
 * Used in nsSMILAnimationController as hash key for mapping an animation
 * target to the nsSMILCompositor for that target.
 *
 * NOTE: Need a nsRefPtr for the element & attribute name, because
 * nsSMILAnimationController retain its hash table for one sample into the
 * future, and we need to make sure their target isn't deleted in that time.
 */

struct nsSMILTargetIdentifier
{
  nsSMILTargetIdentifier()
    : mElement(nsnull), mAttributeName(nsnull),
      mAttributeNamespaceID(kNameSpaceID_Unknown), mIsCSS(false) {}

  inline bool Equals(const nsSMILTargetIdentifier& aOther) const
  {
    return (aOther.mElement              == mElement &&
            aOther.mAttributeName        == mAttributeName &&
            aOther.mAttributeNamespaceID == mAttributeNamespaceID &&
            aOther.mIsCSS                == mIsCSS);
  }

  nsRefPtr<mozilla::dom::Element> mElement;
  nsRefPtr<nsIAtom>    mAttributeName;
  PRInt32              mAttributeNamespaceID;
  bool                 mIsCSS;
};

/**
 * Class: nsSMILWeakTargetIdentifier
 *
 * Version of the above struct that uses non-owning pointers.  These are kept
 * private, to ensure that they aren't ever dereferenced (or used at all,
 * outside of Equals()).
 *
 * This is solely for comparisons to determine if a target has changed
 * from one sample to the next.
 */
class nsSMILWeakTargetIdentifier
{
public:
  // Trivial constructor
  nsSMILWeakTargetIdentifier()
    : mElement(nsnull), mAttributeName(nsnull), mIsCSS(false) {}

  // Allow us to update a weak identifier to match a given non-weak identifier
  nsSMILWeakTargetIdentifier&
    operator=(const nsSMILTargetIdentifier& aOther)
  {
    mElement = aOther.mElement;
    mAttributeName = aOther.mAttributeName;
    mIsCSS = aOther.mIsCSS;
    return *this;
  }

  // Allow for comparison vs. non-weak identifier
  inline bool Equals(const nsSMILTargetIdentifier& aOther) const
  {
    return (aOther.mElement       == mElement &&
            aOther.mAttributeName == mAttributeName &&
            aOther.mIsCSS         == mIsCSS);
  }

private:
  const nsIContent* mElement;
  const nsIAtom*    mAttributeName;
  bool              mIsCSS;
};

#endif // NS_SMILTARGETIDENTIFIER_H_
