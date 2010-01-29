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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSMILInstanceTime.h"

nsSMILInstanceTime::nsSMILInstanceTime(const nsSMILTimeValue& aTime,
    const nsSMILInstanceTime* aDependentTime,
    nsSMILInstanceTimeSource aSource)
: mTime(aTime),
  mFlags(0),
  mSerial(0)
{
  switch (aSource) {
    case SOURCE_NONE:
      // No special flags
      break;

    case SOURCE_DOM:
      mFlags = kClearOnReset | kFromDOM;
      break;

    case SOURCE_SYNCBASE:
      mFlags = kMayUpdate;
      break;

    case SOURCE_EVENT:
      mFlags = kClearOnReset;
      break;
  }

  SetDependentTime(aDependentTime);
}

void
nsSMILInstanceTime::SetDependentTime(const nsSMILInstanceTime* aDependentTime)
{
  // We must make the dependent time mutable because our ref-counting isn't
  // const-correct and BreakPotentialCycle may update dependencies (which should
  // be considered 'mutable')
  nsSMILInstanceTime* mutableDependentTime =
    const_cast<nsSMILInstanceTime*>(aDependentTime);

  // Make sure we don't end up creating a cycle between the dependent time
  // pointers. (Note that this is not the same as detecting syncbase dependency
  // cycles. That is done by nsSMILTimeValueSpec. mDependentTime is used ONLY
  // for ensuring correct ordering within the animation sandwich.)
  if (aDependentTime) {
    mutableDependentTime->BreakPotentialCycle(this);
  }

  mDependentTime = mutableDependentTime;
}

void
nsSMILInstanceTime::BreakPotentialCycle(const nsSMILInstanceTime* aNewTail)
{
  if (!mDependentTime)
    return;

  if (mDependentTime == aNewTail) {
    // Making aNewTail the new tail of the chain would create a cycle so we
    // prevent this by unlinking the pointer to aNewTail.
    mDependentTime = nsnull;
    return;
  }

  mDependentTime->BreakPotentialCycle(aNewTail);
}

PRBool
nsSMILInstanceTime::IsDependent(const nsSMILInstanceTime& aOther,
                                PRUint32 aRecursionDepth) const
{
  NS_ABORT_IF_FALSE(aRecursionDepth < 1000,
      "We seem to have created a cycle between instance times");

  if (!mDependentTime)
    return PR_FALSE;

  if (mDependentTime == &aOther)
    return PR_TRUE;

  return mDependentTime->IsDependent(aOther, ++aRecursionDepth);
}
