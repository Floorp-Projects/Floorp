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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Brian Birtles <birtles@gmail.com>
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

#include "nsSMILNullType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"

/*static*/ nsSMILNullType nsSMILNullType::sSingleton;

nsresult
nsSMILNullType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aSrc.mType == this, "Unexpected source type");
  aDest.mU    = aSrc.mU;
  aDest.mType = &sSingleton;
  return NS_OK;
}

bool
nsSMILNullType::IsEqual(const nsSMILValue& aLeft,
                        const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return PR_TRUE;  // All null-typed values are equivalent.
}

nsresult
nsSMILNullType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                    PRUint32 aCount) const
{
  NS_NOTREACHED("Adding NULL type");
  return NS_ERROR_FAILURE;
}

nsresult
nsSMILNullType::ComputeDistance(const nsSMILValue& aFrom,
                                const nsSMILValue& aTo,
                                double& aDistance) const
{
  NS_NOTREACHED("Computing distance for NULL type");
  return NS_ERROR_FAILURE;
}

nsresult
nsSMILNullType::Interpolate(const nsSMILValue& aStartVal,
                            const nsSMILValue& aEndVal,
                            double aUnitDistance,
                            nsSMILValue& aResult) const
{
  NS_NOTREACHED("Interpolating NULL type");
  return NS_ERROR_FAILURE;
}
