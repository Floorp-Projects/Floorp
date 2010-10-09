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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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
      mAttributeNamespaceID(kNameSpaceID_Unknown), mIsCSS(PR_FALSE) {}

  inline PRBool Equals(const nsSMILTargetIdentifier& aOther) const
  {
    return (aOther.mElement              == mElement &&
            aOther.mAttributeName        == mAttributeName &&
            aOther.mAttributeNamespaceID == mAttributeNamespaceID &&
            aOther.mIsCSS                == mIsCSS);
  }

  nsRefPtr<mozilla::dom::Element> mElement;
  nsRefPtr<nsIAtom>    mAttributeName;
  PRInt32              mAttributeNamespaceID;
  PRPackedBool         mIsCSS;
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
    : mElement(nsnull), mAttributeName(nsnull), mIsCSS(PR_FALSE) {}

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
  inline PRBool Equals(const nsSMILTargetIdentifier& aOther) const
  {
    return (aOther.mElement       == mElement &&
            aOther.mAttributeName == mAttributeName &&
            aOther.mIsCSS         == mIsCSS);
  }

private:
  const nsIContent* mElement;
  const nsIAtom*    mAttributeName;
  PRPackedBool      mIsCSS;
};

#endif // NS_SMILTARGETIDENTIFIER_H_
