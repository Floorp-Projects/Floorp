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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsSMILInterval.h"

nsSMILInterval::nsSMILInterval()
:
  mBeginFixed(PR_FALSE),
  mEndFixed(PR_FALSE)
{
}

nsSMILInterval::nsSMILInterval(const nsSMILInterval& aOther)
:
  mBegin(aOther.mBegin),
  mEnd(aOther.mEnd),
  mBeginFixed(PR_FALSE),
  mEndFixed(PR_FALSE)
{
  NS_ABORT_IF_FALSE(aOther.mDependentTimes.IsEmpty(),
      "Attempting to copy-construct an interval with dependent times, "
      "this will lead to instance times being shared between intervals.");

  // For the time being we don't allow intervals with fixed endpoints to be
  // copied since we only ever copy-construct to establish a new current
  // interval. If we ever need to copy historical intervals we may need to move
  // the ReleaseFixedEndpoint calls from Unlink to the dtor.
  NS_ABORT_IF_FALSE(!aOther.mBeginFixed && !aOther.mEndFixed,
      "Attempting to copy-construct an interval with fixed endpoints");
}

nsSMILInterval::~nsSMILInterval()
{
  NS_ABORT_IF_FALSE(mDependentTimes.IsEmpty(),
      "Destroying interval without disassociating dependent instance times. "
      "Unlink was not called");
}

void
nsSMILInterval::Unlink(PRBool aFiltered)
{
  for (PRInt32 i = mDependentTimes.Length() - 1; i >= 0; --i) {
    if (aFiltered) {
      mDependentTimes[i]->HandleFilteredInterval();
    } else {
      mDependentTimes[i]->HandleDeletedInterval();
    }
  }
  mDependentTimes.Clear();
  if (mBegin && mBeginFixed) {
    mBegin->ReleaseFixedEndpoint();
  }
  mBegin = nsnull;
  if (mEnd && mEndFixed) {
    mEnd->ReleaseFixedEndpoint();
  }
  mEnd = nsnull;
}

nsSMILInstanceTime*
nsSMILInterval::Begin()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting Begin() on un-initialized interval.");
  return mBegin;
}

nsSMILInstanceTime*
nsSMILInterval::End()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting End() on un-initialized interval.");
  return mEnd;
}

void
nsSMILInterval::SetBegin(nsSMILInstanceTime& aBegin)
{
  NS_ABORT_IF_FALSE(aBegin.Time().IsDefinite(),
      "Attempting to set unresolved or indefinite begin time on interval");
  NS_ABORT_IF_FALSE(!mBeginFixed,
      "Attempting to set begin time but the begin point is fixed");
  // Check that we're not making an instance time dependent on itself. Such an
  // arrangement does not make intuitive sense and should be detected when
  // creating or updating intervals.
  NS_ABORT_IF_FALSE(!mBegin || aBegin.GetBaseTime() != mBegin,
      "Attempting to make self-dependent instance time");

  mBegin = &aBegin;
}

void
nsSMILInterval::SetEnd(nsSMILInstanceTime& aEnd)
{
  NS_ABORT_IF_FALSE(!mEndFixed,
      "Attempting to set end time but the end point is fixed");
  // As with SetBegin, check we're not making an instance time dependent on
  // itself.
  NS_ABORT_IF_FALSE(!mEnd || aEnd.GetBaseTime() != mEnd,
      "Attempting to make self-dependent instance time");

  mEnd = &aEnd;
}

void
nsSMILInterval::FixBegin()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Fixing begin point on un-initialized interval");
  NS_ABORT_IF_FALSE(!mBeginFixed, "Duplicate calls to FixBegin()");
  mBeginFixed = PR_TRUE;
  mBegin->AddRefFixedEndpoint();
}

void
nsSMILInterval::FixEnd()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Fixing end point on un-initialized interval");
  NS_ABORT_IF_FALSE(mBeginFixed,
      "Fixing the end of an interval without a fixed begin");
  NS_ABORT_IF_FALSE(!mEndFixed, "Duplicate calls to FixEnd()");
  mEndFixed = PR_TRUE;
  mEnd->AddRefFixedEndpoint();
}

void
nsSMILInterval::AddDependentTime(nsSMILInstanceTime& aTime)
{
  nsRefPtr<nsSMILInstanceTime>* inserted =
    mDependentTimes.InsertElementSorted(&aTime);
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time.");
  }
}

void
nsSMILInterval::RemoveDependentTime(const nsSMILInstanceTime& aTime)
{
#ifdef DEBUG
  PRBool found =
#endif
    mDependentTimes.RemoveElementSorted(&aTime);
  NS_ABORT_IF_FALSE(found, "Couldn't find instance time to delete.");
}

void
nsSMILInterval::GetDependentTimes(InstanceTimeList& aTimes)
{
  aTimes = mDependentTimes;
}

PRBool
nsSMILInterval::IsDependencyChainLink() const
{
  if (!mBegin || !mEnd)
    return PR_FALSE; // Not yet initialised so it can't be part of a chain

  if (mDependentTimes.IsEmpty())
    return PR_FALSE; // No dependents, chain end

  // So we have dependents, but we're still only a link in the chain (as opposed
  // to the end of the chain) if one of our endpoints is dependent on an
  // interval other than ourselves.
  return (mBegin->IsDependent() && mBegin->GetBaseInterval() != this) ||
         (mEnd->IsDependent() && mEnd->GetBaseInterval() != this);
}
